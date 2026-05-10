#include "button_input.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define POWER_HOLD_MS 2000

static long timeval_diff_ms(const struct timeval *a, const struct timeval *b) {
    return (long)(b->tv_sec - a->tv_sec) * 1000L +
           (long)(b->tv_usec - a->tv_usec) / 1000L;
}

static void *button_thread(void *arg) {
    ButtonInput *b = (ButtonInput *)arg;
    struct input_event ev;
    ssize_t n;

    while (b->running) {
        n = read(b->fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        if (ev.type != EV_KEY) continue;

        if (ev.code == KEY_VOLUMEUP && ev.value == 1) {
            b->cb(BUTTON_VOLUME_UP, b->userdata);
        } else if (ev.code == KEY_VOLUMEDOWN && ev.value == 1) {
            b->cb(BUTTON_VOLUME_DOWN, b->userdata);
        } else if (ev.code == KEY_POWER) {
            if (ev.value == 1) {
                b->power_press_time = ev.time;
                b->power_pressed = 1;
            } else if (ev.value == 0 && b->power_pressed) {
                b->power_pressed = 0;
                long ms = timeval_diff_ms(&b->power_press_time, &ev.time);
                if (ms >= POWER_HOLD_MS)
                    b->cb(BUTTON_POWER_HOLD, b->userdata);
                else
                    b->cb(BUTTON_POWER_SHORT, b->userdata);
            }
        }
    }
    return NULL;
}

int button_input_find_device(char *path_out, size_t path_len) {
    char path[64];
    uint8_t keybits[(KEY_MAX + 7) / 8];
    int i;
    for (i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        memset(keybits, 0, sizeof(keybits));
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0) {
            int has_power = (keybits[KEY_POWER / 8] >> (KEY_POWER % 8)) & 1;
            int has_vol   = (keybits[KEY_VOLUMEUP / 8] >> (KEY_VOLUMEUP % 8)) & 1;
            if (has_power && has_vol) {
                close(fd);
                strncpy(path_out, path, path_len - 1);
                path_out[path_len - 1] = '\0';
                return 0;
            }
        }
        close(fd);
    }
    return -1;
}

int button_input_open(ButtonInput *b, const char *event_path,
                      ButtonCallback cb, void *userdata) {
    memset(b, 0, sizeof(*b));
    b->fd = open(event_path, O_RDONLY);
    if (b->fd < 0) return -1;
    b->cb       = cb;
    b->userdata = userdata;
    b->running  = 1;
    if (pthread_create(&b->thread, NULL, button_thread, b) != 0) {
        close(b->fd);
        b->fd = -1;
        return -1;
    }
    return 0;
}

void button_input_close(ButtonInput *b) {
    if (!b->running) return;
    b->running = 0;
    if (b->fd >= 0) {
        close(b->fd);
        b->fd = -1;
    }
    pthread_join(b->thread, NULL);
}