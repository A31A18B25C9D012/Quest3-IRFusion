#ifndef STEREO_DEPTH_H
#define STEREO_DEPTH_H

#include <stdint.h>

typedef struct {
    float focal_px;
    float baseline_m;
    int win_size;
    int max_disparity;
} StereoDepthParams;

void stereo_compute_disparity(const uint8_t *left, const uint8_t *right,
                               int width, int height,
                               const StereoDepthParams *params,
                               float *disparity_out);
void stereo_disparity_to_depth(const float *disparity, int pixel_count,
                                float focal_px, float baseline_m,
                                float *depth_out);

#endif