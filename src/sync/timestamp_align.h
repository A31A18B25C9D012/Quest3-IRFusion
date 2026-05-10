#ifndef TIMESTAMP_ALIGN_H
#define TIMESTAMP_ALIGN_H

#include <stdint.h>
#include <pthread.h>

#define TS_RING_SIZE 32

typedef struct {
    uint8_t *data;
    size_t data_bytes;
    int width;
    int height;
    uint64_t timestamp_ns;
    int valid;
} TSFrame;

typedef struct {
    TSFrame frames[TS_RING_SIZE];
    int head;
    int count;
    int frame_width;
    int frame_height;
    size_t frame_bytes;
    pthread_mutex_t lock;
} TSRingBuffer;

int ts_ring_init(TSRingBuffer *rb, int width, int height, int bytes_per_pixel);
void ts_ring_destroy(TSRingBuffer *rb);
void ts_ring_push(TSRingBuffer *rb, const uint8_t *data, uint64_t timestamp_ns);
int ts_ring_get_nearest(TSRingBuffer *rb, uint64_t query_ns, TSFrame *out);
int ts_ring_interpolate(TSRingBuffer *rb, uint64_t query_ns, uint8_t *out_data, int out_bytes);

#endif