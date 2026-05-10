#ifndef CAMERA_MATH_H
#define CAMERA_MATH_H

#include "mat4.h"

typedef struct {
    float fx, fy, cx, cy;
    float dist[5];
} CameraIntrinsics;

void camera_project(const CameraIntrinsics *K, Vec3 p_cam, float *u, float *v);
Vec3 camera_unproject(const CameraIntrinsics *K, float u, float v, float depth);
float stereo_depth(float focal_px, float baseline_m, float disparity_px);
void homography_apply(const float H[9], float u_in, float v_in, float *u_out, float *v_out);
void homography_invert(const float H[9], float H_inv[9]);
float mat3_det(const float M[9]);
void mat3_mul(const float A[9], const float B[9], float C[9]);

#endif