#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "../math/mat4.h"

typedef struct {
    float fx, fy, cx, cy;
    float dist[5];
} IntrinsicsData;

typedef struct {
    IntrinsicsData K_rgb_left;
    IntrinsicsData K_rgb_right;
    IntrinsicsData K_ir;
    float R_ir[3][3];
    float t_ir[3];
    float H_ir_to_rgb[9];
    float H_inv[9];
    float baseline_m;
    Mat4 T_ir_to_headset;
    int loaded;
} CalibrationData;

int calibration_load(CalibrationData *cal, const char *path);
int calibration_save(const CalibrationData *cal, const char *path);
void calibration_build_transforms(CalibrationData *cal);

#endif