#include "timestamp_align.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int ts_ring_init(TSRingBuffer *rb, int width, int height, int bytes_per_pixel) {
    int i;
    rb->head = 0;
    rb->count = 0;
    rb->frame_width = width;
    rb->frame_height = height;
    rb->frame_bytes = (size_t)(width * height * bytes_per_pixel);
    pthread_mutex_init(&rb->lock, NULL);
    for (i = 0; i < TS_RING_SIZE; i++) {
        rb->frames[i].data = (uint8_t *)malloc(rb->frame_bytes);
        if (!rb->frames[i].data) {
            int j;
            for (j = 0; j < i; j++) free(rb->frames[j].data);
            pthread_mutex_destroy(&rb->lock);
            return -1;
        }
        rb->frames[i].valid = 0;
        rb->frames[i].width = width;
        rb->frames[i].height = height;
        rb->frames[i].data_bytes = rb->frame_bytes;
        rb->frames[i].timestamp_ns = 0;
    }
    return 0;
}

void ts_ring_destroy(TSRingBuffer *rb) {
    int i;
    for (i = 0; i < TS_RING_SIZE; i++)
        if (rb->frames[i].data) free(rb->frames[i].data);
    pthread_mutex_destroy(&rb->lock);
}

void ts_ring_push(TSRingBuffer *rb, const uint8_t *data, uint64_t timestamp_ns) {
    pthread_mutex_lock(&rb->lock);
    int slot = rb->head;
    memcpy(rb->frames[slot].data, data, rb->frame_bytes);
    rb->frames[slot].timestamp_ns = timestamp_ns;
    rb->frames[slot].valid = 1;
    rb->head = (rb->head + 1) % TS_RING_SIZE;
    if (rb->count < TS_RING_SIZE) rb->count++;
    pthread_mutex_unlock(&rb->lock);
}

int ts_ring_get_nearest(TSRingBuffer *rb, uint64_t query_ns, TSFrame *out) {
    pthread_mutex_lock(&rb->lock);
    if (rb->count == 0) {
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }
    int best = -1;
    uint64_t best_diff = UINT64_MAX;
    int i;
    for (i = 0; i < TS_RING_SIZE; i++) {
        if (!rb->frames[i].valid) continue;
        uint64_t t = rb->frames[i].timestamp_ns;
        uint64_t diff = (t > query_ns) ? (t - query_ns) : (query_ns - t);
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }
    if (best < 0) {
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }
    *out = rb->frames[best];
    out->data = NULL;
    pthread_mutex_unlock(&rb->lock);
    out->data = rb->frames[best].data;
    return 0;
}

int ts_ring_interpolate(TSRingBuffer *rb, uint64_t query_ns, uint8_t *out_data, int out_bytes) {
    pthread_mutex_lock(&rb->lock);
    if (rb->count < 2) {
        if (rb->count == 1) {
            int slot = (rb->head - 1 + TS_RING_SIZE) % TS_RING_SIZE;
            if (rb->frames[slot].valid) {
                int copy = (out_bytes < (int)rb->frame_bytes) ? out_bytes : (int)rb->frame_bytes;
                memcpy(out_data, rb->frames[slot].data, copy);
                pthread_mutex_unlock(&rb->lock);
                return 0;
            }
        }
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }

    int prev_idx = -1, next_idx = -1;
    uint64_t prev_t = 0, next_t = UINT64_MAX;
    int i;
    for (i = 0; i < TS_RING_SIZE; i++) {
        if (!rb->frames[i].valid) continue;
        uint64_t t = rb->frames[i].timestamp_ns;
        if (t <= query_ns && t >= prev_t) { prev_t = t; prev_idx = i; }
        if (t > query_ns && t <= next_t) { next_t = t; next_idx = i; }
    }

    if (prev_idx < 0 && next_idx < 0) {
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }
    if (prev_idx < 0) {
        int copy = (out_bytes < (int)rb->frame_bytes) ? out_bytes : (int)rb->frame_bytes;
        memcpy(out_data, rb->frames[next_idx].data, copy);
        pthread_mutex_unlock(&rb->lock);
        return 0;
    }
    if (next_idx < 0) {
        int copy = (out_bytes < (int)rb->frame_bytes) ? out_bytes : (int)rb->frame_bytes;
        memcpy(out_data, rb->frames[prev_idx].data, copy);
        pthread_mutex_unlock(&rb->lock);
        return 0;
    }

    uint64_t span = next_t - prev_t;
    float alpha = (span > 0) ? (float)(query_ns - prev_t) / (float)span : 0.0f;
    float beta = 1.0f - alpha;

    const uint8_t *A = rb->frames[prev_idx].data;
    const uint8_t *B = rb->frames[next_idx].data;
    int n = (out_bytes < (int)rb->frame_bytes) ? out_bytes : (int)rb->frame_bytes;
    for (i = 0; i < n; i++)
        out_data[i] = (uint8_t)(beta * A[i] + alpha * B[i] + 0.5f);

    pthread_mutex_unlock(&rb->lock);
    return 0;
}