#include "stereo_depth.h"
#include "../math/camera_math.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

static uint32_t sad_block(const uint8_t *left, const uint8_t *right,
                           int lx, int ly, int rx, int ry,
                           int width, int half_win) {
    uint32_t sad = 0;
    int dy, dx;
    for (dy = -half_win; dy <= half_win; dy++) {
        const uint8_t *lrow = left  + (ly + dy) * width + lx - half_win;
        const uint8_t *rrow = right + (ry + dy) * width + rx - half_win;
        int n = 2 * half_win + 1;
#ifdef __ARM_NEON
        int chunks = n / 8;
        int rem = n % 8;
        int k;
        uint32x4_t acc = vdupq_n_u32(0);
        for (k = 0; k < chunks; k++) {
            uint8x8_t lv = vld1_u8(lrow + k*8);
            uint8x8_t rv = vld1_u8(rrow + k*8);
            uint8x8_t diff = vabd_u8(lv, rv);
            acc = vaddw_u8(acc, diff);
        }
        uint32x2_t sum2 = vadd_u32(vget_low_u32(acc), vget_high_u32(acc));
        sad += vget_lane_u32(vpadd_u32(sum2, sum2), 0);
        for (k = chunks * 8; k < n; k++)
            sad += (uint32_t)abs((int)lrow[k] - (int)rrow[k]);
#else
        int k;
        for (k = 0; k < n; k++)
            sad += (uint32_t)abs((int)lrow[k] - (int)rrow[k]);
#endif
    }
    return sad;
}

void stereo_compute_disparity(const uint8_t *left, const uint8_t *right,
                               int width, int height,
                               const StereoDepthParams *params,
                               float *disparity_out) {
    int half_win = params->win_size / 2;
    int max_disp = params->max_disparity;
    int x, y, d;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (y < half_win || y >= height - half_win ||
                x < half_win || x >= width  - half_win) {
                disparity_out[y * width + x] = 0.0f;
                continue;
            }
            int best_d = 0;
            uint32_t best_sad = UINT32_MAX;
            for (d = 0; d < max_disp; d++) {
                int rx = x - d;
                if (rx - half_win < 0) break;
                uint32_t s = sad_block(left, right, x, y, rx, y, width, half_win);
                if (s < best_sad) { best_sad = s; best_d = d; }
            }
            disparity_out[y * width + x] = (float)best_d;
        }
    }
}

void stereo_disparity_to_depth(const float *disparity, int pixel_count,
                                float focal_px, float baseline_m,
                                float *depth_out) {
    int i;
    for (i = 0; i < pixel_count; i++)
        depth_out[i] = stereo_depth(focal_px, baseline_m, disparity[i]);
}