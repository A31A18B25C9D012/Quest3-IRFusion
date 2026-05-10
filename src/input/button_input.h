#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    BUTTON_VOLUME_UP,
    BUTTON_VOLUME_DOWN,
    BUTTON_POWER_SHORT,
    BUTTON_POWER_HOLD,
} ButtonEvent;

typedef void (*ButtonCallback)(ButtonEvent event, void *userdata);

typedef struct {
    int fd;
    int write_pipe_fd;
    pthread_t thread;
    volatile int running;
    struct timeval power_press_time;
    int power_pressed;
    ButtonCallback cb;
    void *userdata;
} ButtonInput;

int  button_input_find_device(char *path_out, size_t path_len);
int  button_input_open(ButtonInput *b, const char *event_path,
                       ButtonCallback cb, void *userdata);
void button_input_close(ButtonInput *b);

#endif