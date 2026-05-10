#include "camera_math.h"
#include <math.h>
#include <string.h>

void camera_project(const CameraIntrinsics *K, Vec3 p_cam, float *u, float *v) {
    if (p_cam.v[2] <= 0.0f) {
        *u = *v = -1.0f;
        return;
    }
    float x = p_cam.v[0] / p_cam.v[2];
    float y = p_cam.v[1] / p_cam.v[2];
    float r2 = x*x + y*y;
    float r4 = r2*r2;
    float r6 = r4*r2;
    float radial = 1.0f + K->dist[0]*r2 + K->dist[1]*r4 + K->dist[4]*r6;
    float xd = x*radial + 2.0f*K->dist[2]*x*y + K->dist[3]*(r2 + 2.0f*x*x);
    float yd = y*radial + K->dist[2]*(r2 + 2.0f*y*y) + 2.0f*K->dist[3]*x*y;
    *u = K->fx * xd + K->cx;
    *v = K->fy * yd + K->cy;
}

Vec3 camera_unproject(const CameraIntrinsics *K, float u, float v, float depth) {
    float x = (u - K->cx) / K->fx;
    float y = (v - K->cy) / K->fy;
    Vec3 result = {{x * depth, y * depth, depth}};
    return result;
}

float stereo_depth(float focal_px, float baseline_m, float disparity_px) {
    if (disparity_px <= 0.0f)
        return 0.0f;
    return (focal_px * baseline_m) / disparity_px;
}

void homography_apply(const float H[9], float u_in, float v_in, float *u_out, float *v_out) {
    float w = H[6]*u_in + H[7]*v_in + H[8];
    if (w < 1e-10f && w > -1e-10f) {
        *u_out = *v_out = 0.0f;
        return;
    }
    *u_out = (H[0]*u_in + H[1]*v_in + H[2]) / w;
    *v_out = (H[3]*u_in + H[4]*v_in + H[5]) / w;
}

float mat3_det(const float M[9]) {
    return M[0]*(M[4]*M[8]-M[5]*M[7])
          -M[1]*(M[3]*M[8]-M[5]*M[6])
          +M[2]*(M[3]*M[7]-M[4]*M[6]);
}

void mat3_mul(const float A[9], const float B[9], float C[9]) {
    int i, j, k;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            float s = 0.0f;
            for (k = 0; k < 3; k++)
                s += A[i*3+k] * B[k*3+j];
            C[i*3+j] = s;
        }
    }
}

void homography_invert(const float H[9], float H_inv[9]) {
    float det = mat3_det(H);
    if (det > -1e-10f && det < 1e-10f) {
        int i;
        for (i = 0; i < 9; i++) H_inv[i] = 0.0f;
        H_inv[0] = H_inv[4] = H_inv[8] = 1.0f;
        return;
    }
    float idet = 1.0f / det;
    H_inv[0] =  (H[4]*H[8]-H[5]*H[7]) * idet;
    H_inv[1] = -(H[1]*H[8]-H[2]*H[7]) * idet;
    H_inv[2] =  (H[1]*H[5]-H[2]*H[4]) * idet;
    H_inv[3] = -(H[3]*H[8]-H[5]*H[6]) * idet;
    H_inv[4] =  (H[0]*H[8]-H[2]*H[6]) * idet;
    H_inv[5] = -(H[0]*H[5]-H[2]*H[3]) * idet;
    H_inv[6] =  (H[3]*H[7]-H[4]*H[6]) * idet;
    H_inv[7] = -(H[0]*H[7]-H[1]*H[6]) * idet;
    H_inv[8] =  (H[0]*H[4]-H[1]*H[3]) * idet;
}