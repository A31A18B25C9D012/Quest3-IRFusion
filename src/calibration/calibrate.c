#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/calib3d/calib3d_c.h>
#include <opencv2/highgui/highgui_c.h>
#include "../calibration/calibration.h"

#define MAX_CAPTURES 40
#define BOARD_W 9
#define BOARD_H 6
#define SQUARE_MM 30.0f

static void make_object_points(CvPoint3D32f *pts, int count) {
    int i;
    for (i = 0; i < count; i++) {
        pts[i].x = (float)(i % BOARD_W) * SQUARE_MM;
        pts[i].y = (float)(i / BOARD_W) * SQUARE_MM;
        pts[i].z = 0.0f;
    }
}

static int detect_corners(IplImage *gray, CvPoint2D32f *corners, int *corner_count) {
    int total = BOARD_W * BOARD_H;
    int found = cvFindChessboardCorners(gray, cvSize(BOARD_W, BOARD_H),
                                        corners, corner_count,
                                        CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
    if (found && *corner_count == total) {
        IplImage *gray32 = cvCloneImage(gray);
        CvTermCriteria crit = cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 30, 0.1);
        cvFindCornerSubPix(gray32, corners, *corner_count, cvSize(11,11), cvSize(-1,-1), crit);
        cvReleaseImage(&gray32);
        return 1;
    }
    return 0;
}

static void calibrate_single(IplImage **frames, int n_frames,
                              int img_w, int img_h,
                              float *fx, float *fy, float *cx, float *cy, float dist[5]) {
    int total = BOARD_W * BOARD_H;
    int valid = 0, f;
    CvPoint2D32f *img_pts_buf = (CvPoint2D32f *)malloc(n_frames * total * sizeof(CvPoint2D32f));
    CvPoint3D32f *obj_pts_buf = (CvPoint3D32f *)malloc(n_frames * total * sizeof(CvPoint3D32f));
    int *pt_counts = (int *)malloc(n_frames * sizeof(int));
    CvPoint3D32f row[BOARD_W * BOARD_H];
    make_object_points(row, total);

    for (f = 0; f < n_frames; f++) {
        CvPoint2D32f corners[BOARD_W * BOARD_H];
        int corner_count = 0;
        if (detect_corners(frames[f], corners, &corner_count)) {
            memcpy(&img_pts_buf[valid * total], corners, total * sizeof(CvPoint2D32f));
            memcpy(&obj_pts_buf[valid * total], row, total * sizeof(CvPoint3D32f));
            pt_counts[valid] = total;
            valid++;
        }
    }

    if (valid < 3) {
        fprintf(stderr, "insufficient valid frames for calibration: %d\n", valid);
        free(img_pts_buf); free(obj_pts_buf); free(pt_counts);
        return;
    }

    CvMat obj_pts = cvMat(1, valid * total, CV_32FC3, obj_pts_buf);
    CvMat img_pts = cvMat(1, valid * total, CV_32FC2, img_pts_buf);
    CvMat pt_cnt_mat = cvMat(1, valid, CV_32SC1, pt_counts);

    float K_data[9] = {0};
    float D_data[5] = {0};
    CvMat K_mat = cvMat(3, 3, CV_32F, K_data);
    CvMat D_mat = cvMat(1, 5, CV_32F, D_data);

    cvCalibrateCamera2(&obj_pts, &img_pts, &pt_cnt_mat,
                       cvSize(img_w, img_h), &K_mat, &D_mat,
                       NULL, NULL, CV_CALIB_FIX_K3);

    *fx = K_data[0]; *fy = K_data[4]; *cx = K_data[2]; *cy = K_data[5];
    memcpy(dist, D_data, 5 * sizeof(float));

    free(img_pts_buf); free(obj_pts_buf); free(pt_counts);
}

static void stereo_calibrate_pair(
    IplImage **rgb_frames, IplImage **ir_frames, int n_frames,
    int rgb_w, int rgb_h,
    float rgb_fx, float rgb_fy, float rgb_cx, float rgb_cy, float rgb_dist[5],
    float ir_fx, float ir_fy, float ir_cx, float ir_cy, float ir_dist[5],
    float R_out[3][3], float t_out[3], float H_out[9])
{
    int total = BOARD_W * BOARD_H;
    int valid = 0, f;
    CvPoint2D32f *rgb_pts = (CvPoint2D32f *)malloc(n_frames * total * sizeof(CvPoint2D32f));
    CvPoint2D32f *ir_pts  = (CvPoint2D32f *)malloc(n_frames * total * sizeof(CvPoint2D32f));
    CvPoint3D32f *obj_pts = (CvPoint3D32f *)malloc(n_frames * total * sizeof(CvPoint3D32f));
    int *pt_counts = (int *)malloc(n_frames * sizeof(int));
    CvPoint3D32f row[BOARD_W * BOARD_H];
    make_object_points(row, total);

    for (f = 0; f < n_frames; f++) {
        CvPoint2D32f rgb_corners[BOARD_W * BOARD_H];
        CvPoint2D32f ir_corners[BOARD_W * BOARD_H];
        int rc = 0, ic = 0;
        int rgb_found = detect_corners(rgb_frames[f], rgb_corners, &rc);
        int ir_found  = detect_corners(ir_frames[f],  ir_corners,  &ic);
        if (rgb_found && ir_found && rc == total && ic == total) {
            memcpy(&rgb_pts[valid * total], rgb_corners, total * sizeof(CvPoint2D32f));
            memcpy(&ir_pts[valid * total],  ir_corners,  total * sizeof(CvPoint2D32f));
            memcpy(&obj_pts[valid * total], row,          total * sizeof(CvPoint3D32f));
            pt_counts[valid] = total;
            valid++;
        }
    }

    if (valid < 3) {
        fprintf(stderr, "insufficient valid stereo pairs: %d\n", valid);
        free(rgb_pts); free(ir_pts); free(obj_pts); free(pt_counts);
        return;
    }

    float K_rgb_data[9] = { rgb_fx, 0, rgb_cx, 0, rgb_fy, rgb_cy, 0, 0, 1 };
    float K_ir_data[9]  = { ir_fx,  0, ir_cx,  0, ir_fy,  ir_cy,  0, 0, 1 };
    float R_data[9] = {0}; float t_data[3] = {0};
    float E_data[9] = {0}; float F_data[9] = {0};

    CvMat obj_mat = cvMat(1, valid * total, CV_32FC3, obj_pts);
    CvMat rgb_mat = cvMat(1, valid * total, CV_32FC2, rgb_pts);
    CvMat ir_mat  = cvMat(1, valid * total, CV_32FC2, ir_pts);
    CvMat cnt_mat = cvMat(1, valid, CV_32SC1, pt_counts);
    CvMat K_rgb = cvMat(3, 3, CV_32F, K_rgb_data);
    CvMat K_ir  = cvMat(3, 3, CV_32F, K_ir_data);
    CvMat D_rgb = cvMat(1, 5, CV_32F, rgb_dist);
    CvMat D_ir  = cvMat(1, 5, CV_32F, ir_dist);
    CvMat R_mat = cvMat(3, 3, CV_32F, R_data);
    CvMat t_mat = cvMat(3, 1, CV_32F, t_data);
    CvMat E_mat = cvMat(3, 3, CV_32F, E_data);
    CvMat F_mat = cvMat(3, 3, CV_32F, F_data);

    cvStereoCalibrate(&obj_mat, &ir_mat, &rgb_mat, &cnt_mat,
                      &K_ir, &D_ir, &K_rgb, &D_rgb,
                      cvSize(rgb_w, rgb_h),
                      &R_mat, &t_mat, &E_mat, &F_mat,
                      cvTermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 100, 1e-5),
                      CV_CALIB_FIX_INTRINSIC);

    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            R_out[i][j] = R_data[i*3+j];
    t_out[0] = t_data[0]; t_out[1] = t_data[1]; t_out[2] = t_data[2];

    CvMat *ir_pts_h  = cvCreateMat(1, valid * total, CV_32FC2);
    CvMat *rgb_pts_h = cvCreateMat(1, valid * total, CV_32FC2);
    memcpy(ir_pts_h->data.ptr, ir_pts, valid * total * sizeof(CvPoint2D32f));
    memcpy(rgb_pts_h->data.ptr, rgb_pts, valid * total * sizeof(CvPoint2D32f));
    CvMat H_mat = cvMat(3, 3, CV_32F, H_out);
    cvFindHomography(ir_pts_h, rgb_pts_h, &H_mat, CV_RANSAC, 3.0, NULL);
    cvReleaseMat(&ir_pts_h);
    cvReleaseMat(&rgb_pts_h);

    free(rgb_pts); free(ir_pts); free(obj_pts); free(pt_counts);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: calibrate <rgb_device_index> <ir_device_index> [output.json]\n");
        return 1;
    }

    int rgb_dev = atoi(argv[1]);
    int ir_dev  = atoi(argv[2]);
    const char *out_path = (argc >= 4) ? argv[3] : "calibration.json";

    CvCapture *rgb_cap = cvCaptureFromCAM(rgb_dev);
    CvCapture *ir_cap  = cvCaptureFromCAM(ir_dev);
    if (!rgb_cap || !ir_cap) {
        fprintf(stderr, "failed to open cameras\n");
        return 1;
    }

    cvSetCaptureProperty(rgb_cap, CV_CAP_PROP_FRAME_WIDTH, 1280);
    cvSetCaptureProperty(rgb_cap, CV_CAP_PROP_FRAME_HEIGHT, 720);
    cvSetCaptureProperty(ir_cap, CV_CAP_PROP_FRAME_WIDTH, 1280);
    cvSetCaptureProperty(ir_cap, CV_CAP_PROP_FRAME_HEIGHT, 720);

    IplImage *rgb_frames[MAX_CAPTURES];
    IplImage *ir_frames[MAX_CAPTURES];
    int n_captured = 0;
    int rgb_w = 0, rgb_h = 0, ir_w = 0, ir_h = 0;

    fprintf(stdout, "press SPACE to capture, ESC to finish\n");

    while (n_captured < MAX_CAPTURES) {
        IplImage *rgb_raw = cvQueryFrame(rgb_cap);
        IplImage *ir_raw  = cvQueryFrame(ir_cap);
        if (!rgb_raw || !ir_raw) continue;

        if (rgb_w == 0) {
            rgb_w = rgb_raw->width; rgb_h = rgb_raw->height;
            ir_w  = ir_raw->width;  ir_h  = ir_raw->height;
        }

        cvShowImage("RGB", rgb_raw);
        cvShowImage("IR",  ir_raw);

        int key = cvWaitKey(30) & 0xFF;
        if (key == 27) break;
        if (key == ' ') {
            IplImage *rgb_gray = cvCreateImage(cvSize(rgb_w, rgb_h), IPL_DEPTH_8U, 1);
            IplImage *ir_gray  = cvCreateImage(cvSize(ir_w,  ir_h),  IPL_DEPTH_8U, 1);
            cvCvtColor(rgb_raw, rgb_gray, CV_BGR2GRAY);
            if (ir_raw->nChannels == 3)
                cvCvtColor(ir_raw, ir_gray, CV_BGR2GRAY);
            else
                cvCopy(ir_raw, ir_gray, NULL);
            rgb_frames[n_captured] = rgb_gray;
            ir_frames[n_captured]  = ir_gray;
            n_captured++;
            fprintf(stdout, "captured %d / %d\n", n_captured, MAX_CAPTURES);
        }
    }

    cvReleaseCapture(&rgb_cap);
    cvReleaseCapture(&ir_cap);
    cvDestroyAllWindows();

    if (n_captured < 5) {
        fprintf(stderr, "need at least 5 captures\n");
        return 1;
    }

    CalibrationData cal;
    memset(&cal, 0, sizeof(cal));

    calibrate_single(rgb_frames, n_captured, rgb_w, rgb_h,
                     &cal.K_rgb_left.fx, &cal.K_rgb_left.fy,
                     &cal.K_rgb_left.cx, &cal.K_rgb_left.cy,
                     cal.K_rgb_left.dist);

    calibrate_single(ir_frames, n_captured, ir_w, ir_h,
                     &cal.K_ir.fx, &cal.K_ir.fy,
                     &cal.K_ir.cx, &cal.K_ir.cy,
                     cal.K_ir.dist);

    stereo_calibrate_pair(rgb_frames, ir_frames, n_captured,
                          rgb_w, rgb_h,
                          cal.K_rgb_left.fx, cal.K_rgb_left.fy,
                          cal.K_rgb_left.cx, cal.K_rgb_left.cy,
                          cal.K_rgb_left.dist,
                          cal.K_ir.fx, cal.K_ir.fy,
                          cal.K_ir.cx, cal.K_ir.cy,
                          cal.K_ir.dist,
                          cal.R_ir, cal.t_ir, cal.H_ir_to_rgb);

    cal.baseline_m = 0.065f;
    calibration_build_transforms(&cal);
    calibration_save(&cal, out_path);
    fprintf(stdout, "calibration saved to %s\n", out_path);

    int i;
    for (i = 0; i < n_captured; i++) {
        cvReleaseImage(&rgb_frames[i]);
        cvReleaseImage(&ir_frames[i]);
    }
    return 0;
}