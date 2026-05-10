#include "lens_correction.h"
#include <string.h>

void lens_coeffs_init_identity(LensCoeffs *lc) {
    lc->k1 = 0.0f;
    lc->k2 = 0.0f;
}

void lens_coeffs_init_quest3(LensCoeffs *lc) {
    lc->k1 = 0.0f;
    lc->k2 = 0.0f;
}

void lens_get_push_constants(const LensCoeffs *lc, int width, int height,
                             LensPushConstants *out) {
    out->k1     = lc->k1;
    out->k2     = lc->k2;
    out->width  = width;
    out->height = height;
}