#ifndef LENS_CORRECTION_H
#define LENS_CORRECTION_H

typedef struct {
    float k1;
    float k2;
} LensCoeffs;

typedef struct {
    float k1;
    float k2;
    int   width;
    int   height;
} LensPushConstants;

void lens_coeffs_init_identity(LensCoeffs *lc);
void lens_coeffs_init_quest3(LensCoeffs *lc);
void lens_get_push_constants(const LensCoeffs *lc, int width, int height,
                             LensPushConstants *out);

#endif