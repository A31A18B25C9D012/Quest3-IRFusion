#include "ir_overlay.h"
#include <string.h>

static const float MODE_COEFFS[5][4] = {
    { 1.0f, 0.0f, 0.0f, 0.5f },
    { 0.8f, 0.5f, 0.0f, 0.5f },
    { 0.0f, 1.0f, 0.0f, 0.5f },
    { 0.9f, 0.0f, 0.5f, 0.5f },
    { 0.8f, 0.4f, 0.1f, 0.8f }
};

void ir_overlay_init(IROverlayState *s) {
    s->mode = IR_MODE_IR_OVERLAY;
    ir_overlay_set_mode(s, IR_MODE_IR_OVERLAY);
}

void ir_overlay_set_mode(IROverlayState *s, IROverlayMode mode) {
    int m = (int)mode;
    if (m < 0 || m > 4) m = 1;
    s->mode = (IROverlayMode)m;
    s->alpha = MODE_COEFFS[m][0];
    s->beta  = MODE_COEFFS[m][1];
    s->gamma = MODE_COEFFS[m][2];
    s->depth_k = MODE_COEFFS[m][3];
}

void ir_overlay_get_coefficients(const IROverlayState *s, float *a, float *b, float *g, float *dk) {
    *a = s->alpha;
    *b = s->beta;
    *g = s->gamma;
    *dk = s->depth_k;
}