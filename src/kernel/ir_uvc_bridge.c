#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/dma-buf.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("IRFusion");
MODULE_DESCRIPTION("IR UVC camera bridge with DMABUF support");
MODULE_VERSION("1.0");

#define DRIVER_NAME "ir_uvc_bridge"
#define MAX_FRAME_BUFS 8

struct ir_uvc_buf {
    struct vb2_buffer vb;
    struct list_head list;
};

struct ir_uvc_dev {
    struct usb_device *udev;
    struct usb_interface *intf;
    struct v4l2_device v4l2_dev;
    struct video_device vdev;
    struct vb2_queue vb2_q;
    struct list_head buf_list;
    spinlock_t buf_lock;
    struct mutex lock;
    int width;
    int height;
    int pixelformat;
    struct usb_anchor submitted;
    int streaming;
    int urb_count;
    struct urb *urbs[8];
    size_t frame_size;
};

static const struct usb_device_id ir_uvc_table[] = {
    { USB_DEVICE(0x289d, 0x0010) },
    { USB_DEVICE(0x1e4e, 0x0100) },
    { USB_DEVICE(0x0bda, 0x5830) },
    { }
};
MODULE_DEVICE_TABLE(usb, ir_uvc_table);

static int ir_queue_setup(struct vb2_queue *q, unsigned int *num_buffers,
                           unsigned int *num_planes, unsigned int sizes[],
                           struct device *alloc_devs[]) {
    struct ir_uvc_dev *dev = vb2_get_drv_priv(q);
    if (*num_buffers < 2) *num_buffers = 2;
    if (*num_buffers > MAX_FRAME_BUFS) *num_buffers = MAX_FRAME_BUFS;
    *num_planes = 1;
    sizes[0] = dev->frame_size;
    return 0;
}

static void ir_buf_prepare(struct vb2_buffer *vb) {
    struct ir_uvc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
    vb2_set_plane_payload(vb, 0, dev->frame_size);
}

static void ir_buf_queue(struct vb2_buffer *vb) {
    struct ir_uvc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
    struct ir_uvc_buf *buf = container_of(vb, struct ir_uvc_buf, vb);
    unsigned long flags;
    spin_lock_irqsave(&dev->buf_lock, flags);
    list_add_tail(&buf->list, &dev->buf_list);
    spin_unlock_irqrestore(&dev->buf_lock, flags);
}

static void ir_urb_complete(struct urb *urb) {
    struct ir_uvc_dev *dev = urb->context;
    struct ir_uvc_buf *buf;
    unsigned long flags;
    void *dst;

    if (urb->status != 0) {
        if (urb->status == -ENOENT || urb->status == -ECONNRESET || urb->status == -ESHUTDOWN)
            return;
        usb_submit_urb(urb, GFP_ATOMIC);
        return;
    }

    spin_lock_irqsave(&dev->buf_lock, flags);
    if (list_empty(&dev->buf_list)) {
        spin_unlock_irqrestore(&dev->buf_lock, flags);
        usb_submit_urb(urb, GFP_ATOMIC);
        return;
    }
    buf = list_first_entry(&dev->buf_list, struct ir_uvc_buf, list);
    list_del(&buf->list);
    spin_unlock_irqrestore(&dev->buf_lock, flags);

    dst = vb2_plane_vaddr(&buf->vb, 0);
    if (dst && urb->actual_length <= (int)dev->frame_size)
        memcpy(dst, urb->transfer_buffer, urb->actual_length);

    vb2_set_plane_payload(&buf->vb, 0, urb->actual_length);
    buf->vb.timestamp = ktime_get_ns();
    vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);

    usb_submit_urb(urb, GFP_ATOMIC);
}

static int ir_start_streaming(struct vb2_queue *q, unsigned int count) {
    struct ir_uvc_dev *dev = vb2_get_drv_priv(q);
    int i, ret;
    size_t buf_size = dev->frame_size;

    for (i = 0; i < 4; i++) {
        dev->urbs[i] = usb_alloc_urb(0, GFP_KERNEL);
        if (!dev->urbs[i]) { ret = -ENOMEM; goto err; }
        dev->urbs[i]->transfer_buffer = kmalloc(buf_size, GFP_KERNEL);
        if (!dev->urbs[i]->transfer_buffer) { ret = -ENOMEM; goto err; }
        usb_fill_bulk_urb(dev->urbs[i], dev->udev,
                          usb_rcvbulkpipe(dev->udev, 0x81),
                          dev->urbs[i]->transfer_buffer, (int)buf_size,
                          ir_urb_complete, dev);
        usb_anchor_urb(dev->urbs[i], &dev->submitted);
        ret = usb_submit_urb(dev->urbs[i], GFP_KERNEL);
        if (ret) goto err;
    }
    dev->streaming = 1;
    dev->urb_count = 4;
    return 0;
err:
    for (i = 0; i < 4; i++) {
        if (dev->urbs[i]) {
            if (dev->urbs[i]->transfer_buffer)
                kfree(dev->urbs[i]->transfer_buffer);
            usb_free_urb(dev->urbs[i]);
            dev->urbs[i] = NULL;
        }
    }
    return ret;
}

static void ir_stop_streaming(struct vb2_queue *q) {
    struct ir_uvc_dev *dev = vb2_get_drv_priv(q);
    struct ir_uvc_buf *buf;
    unsigned long flags;
    int i;

    dev->streaming = 0;
    usb_kill_anchored_urbs(&dev->submitted);

    for (i = 0; i < dev->urb_count; i++) {
        if (dev->urbs[i]) {
            kfree(dev->urbs[i]->transfer_buffer);
            usb_free_urb(dev->urbs[i]);
            dev->urbs[i] = NULL;
        }
    }

    spin_lock_irqsave(&dev->buf_lock, flags);
    while (!list_empty(&dev->buf_list)) {
        buf = list_first_entry(&dev->buf_list, struct ir_uvc_buf, list);
        list_del(&buf->list);
        vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
    }
    spin_unlock_irqrestore(&dev->buf_lock, flags);
}

static const struct vb2_ops ir_vb2_ops = {
    .queue_setup     = ir_queue_setup,
    .buf_prepare     = ir_buf_prepare,
    .buf_queue       = ir_buf_queue,
    .start_streaming = ir_start_streaming,
    .stop_streaming  = ir_stop_streaming,
};

static int ir_querycap(struct file *file, void *priv, struct v4l2_capability *cap) {
    strscpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
    strscpy(cap->card, "IR UVC Bridge", sizeof(cap->card));
    cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
    return 0;
}

static int ir_g_fmt(struct file *file, void *priv, struct v4l2_format *f) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    f->fmt.pix.width  = dev->width;
    f->fmt.pix.height = dev->height;
    f->fmt.pix.pixelformat = dev->pixelformat;
    f->fmt.pix.field = V4L2_FIELD_NONE;
    f->fmt.pix.bytesperline = (uint32_t)(dev->width);
    f->fmt.pix.sizeimage = (uint32_t)(dev->frame_size);
    return 0;
}

static int ir_s_fmt(struct file *file, void *priv, struct v4l2_format *f) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    dev->width  = (int)f->fmt.pix.width;
    dev->height = (int)f->fmt.pix.height;
    dev->pixelformat = (int)f->fmt.pix.pixelformat;
    dev->frame_size = (size_t)(dev->width * dev->height);
    f->fmt.pix.bytesperline = (uint32_t)(dev->width);
    f->fmt.pix.sizeimage = (uint32_t)(dev->frame_size);
    return 0;
}

static int ir_reqbufs(struct file *file, void *priv, struct v4l2_requestbuffers *rb) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_reqbufs(&dev->vb2_q, rb);
}

static int ir_querybuf(struct file *file, void *priv, struct v4l2_buffer *b) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_querybuf(&dev->vb2_q, b);
}

static int ir_qbuf(struct file *file, void *priv, struct v4l2_buffer *b) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_qbuf(&dev->vb2_q, NULL, b);
}

static int ir_dqbuf(struct file *file, void *priv, struct v4l2_buffer *b) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_dqbuf(&dev->vb2_q, b, file->f_flags & O_NONBLOCK);
}

static int ir_streamon(struct file *file, void *priv, enum v4l2_buf_type t) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_streamon(&dev->vb2_q, t);
}

static int ir_streamoff(struct file *file, void *priv, enum v4l2_buf_type t) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_streamoff(&dev->vb2_q, t);
}

static int ir_expbuf(struct file *file, void *priv, struct v4l2_exportbuffer *eb) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return vb2_expbuf(&dev->vb2_q, eb);
}

static const struct v4l2_ioctl_ops ir_ioctl_ops = {
    .vidioc_querycap      = ir_querycap,
    .vidioc_g_fmt_vid_cap = ir_g_fmt,
    .vidioc_s_fmt_vid_cap = ir_s_fmt,
    .vidioc_try_fmt_vid_cap = ir_g_fmt,
    .vidioc_reqbufs       = ir_reqbufs,
    .vidioc_querybuf      = ir_querybuf,
    .vidioc_qbuf          = ir_qbuf,
    .vidioc_dqbuf         = ir_dqbuf,
    .vidioc_streamon      = ir_streamon,
    .vidioc_streamoff     = ir_streamoff,
    .vidioc_expbuf        = ir_expbuf,
};

static int ir_open(struct file *file) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    return v4l2_fh_open(file) ? -ENOMEM : mutex_lock_interruptible(&dev->lock);
}

static int ir_release(struct file *file) {
    struct ir_uvc_dev *dev = video_drvdata(file);
    mutex_unlock(&dev->lock);
    vb2_fop_release(file);
    return 0;
}

static const struct v4l2_file_operations ir_fops = {
    .owner          = THIS_MODULE,
    .open           = ir_open,
    .release        = ir_release,
    .unlocked_ioctl = video_ioctl2,
    .mmap           = vb2_fop_mmap,
    .poll           = vb2_fop_poll,
};

static int ir_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    struct ir_uvc_dev *dev;
    int ret;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;

    dev->udev = usb_get_dev(interface_to_usbdev(intf));
    dev->intf = intf;
    dev->width = 160;
    dev->height = 120;
    dev->pixelformat = V4L2_PIX_FMT_GREY;
    dev->frame_size = (size_t)(dev->width * dev->height);

    mutex_init(&dev->lock);
    spin_lock_init(&dev->buf_lock);
    INIT_LIST_HEAD(&dev->buf_list);
    init_usb_anchor(&dev->submitted);

    ret = v4l2_device_register(&intf->dev, &dev->v4l2_dev);
    if (ret) goto err_free;

    dev->vb2_q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dev->vb2_q.io_modes = VB2_MMAP | VB2_DMABUF;
    dev->vb2_q.drv_priv = dev;
    dev->vb2_q.buf_struct_size = sizeof(struct ir_uvc_buf);
    dev->vb2_q.ops = &ir_vb2_ops;
    dev->vb2_q.mem_ops = &vb2_vmalloc_memops;
    dev->vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
    dev->vb2_q.min_buffers_needed = 2;
    dev->vb2_q.lock = &dev->lock;
    ret = vb2_queue_init(&dev->vb2_q);
    if (ret) goto err_v4l2;

    dev->vdev.v4l2_dev = &dev->v4l2_dev;
    dev->vdev.fops = &ir_fops;
    dev->vdev.ioctl_ops = &ir_ioctl_ops;
    dev->vdev.release = video_device_release_empty;
    dev->vdev.queue = &dev->vb2_q;
    dev->vdev.lock = &dev->lock;
    strscpy(dev->vdev.name, DRIVER_NAME, sizeof(dev->vdev.name));
    video_set_drvdata(&dev->vdev, dev);

    ret = video_register_device(&dev->vdev, VFL_TYPE_VIDEO, -1);
    if (ret) goto err_v4l2;

    usb_set_intfdata(intf, dev);
    dev_info(&intf->dev, "registered as /dev/video%d\n", dev->vdev.num);
    return 0;

err_v4l2:
    v4l2_device_unregister(&dev->v4l2_dev);
err_free:
    usb_put_dev(dev->udev);
    kfree(dev);
    return ret;
}

static void ir_disconnect(struct usb_interface *intf) {
    struct ir_uvc_dev *dev = usb_get_intfdata(intf);
    if (!dev) return;
    usb_kill_anchored_urbs(&dev->submitted);
    video_unregister_device(&dev->vdev);
    v4l2_device_unregister(&dev->v4l2_dev);
    usb_put_dev(dev->udev);
    kfree(dev);
}

static struct usb_driver ir_uvc_driver = {
    .name       = DRIVER_NAME,
    .probe      = ir_probe,
    .disconnect = ir_disconnect,
    .id_table   = ir_uvc_table,
};

module_usb_driver(ir_uvc_driver);