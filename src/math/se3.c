#include "se3.h"
#include "mat4.h"
#include <math.h>
#include <string.h>

Mat4 se3_from_rt(float R[3][3], float t[3]) {
    Mat4 T = mat4_identity();
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            T.m[i][j] = R[i][j];
    T.m[0][3] = t[0];
    T.m[1][3] = t[1];
    T.m[2][3] = t[2];
    return T;
}

Mat4 se3_invert(Mat4 T) {
    Mat4 result = mat4_identity();
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            result.m[i][j] = T.m[j][i];
    for (i = 0; i < 3; i++) {
        result.m[i][3] = 0.0f;
        for (j = 0; j < 3; j++)
            result.m[i][3] -= result.m[i][j] * T.m[j][3];
    }
    return result;
}

Mat4 se3_compose(Mat4 T1, Mat4 T2) {
    return mat4_mul(T1, T2);
}

Vec3 se3_apply(Mat4 T, Vec3 p) {
    return mat4_apply_point(T, p);
}

void se3_to_rt(Mat4 T, float R_out[3][3], float t_out[3]) {
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            R_out[i][j] = T.m[i][j];
    t_out[0] = T.m[0][3];
    t_out[1] = T.m[1][3];
    t_out[2] = T.m[2][3];
}

Mat4 se3_from_rotvec(float rx, float ry, float rz, float tx, float ty, float tz) {
    float angle = sqrtf(rx*rx + ry*ry + rz*rz);
    Mat4 T = mat4_identity();
    T.m[0][3] = tx;
    T.m[1][3] = ty;
    T.m[2][3] = tz;
    if (angle < 1e-10f)
        return T;
    float nx = rx / angle;
    float ny = ry / angle;
    float nz = rz / angle;
    float c = cosf(angle);
    float s = sinf(angle);
    float one_minus_c = 1.0f - c;
    T.m[0][0] = c + nx*nx*one_minus_c;
    T.m[0][1] = nx*ny*one_minus_c - nz*s;
    T.m[0][2] = nx*nz*one_minus_c + ny*s;
    T.m[1][0] = ny*nx*one_minus_c + nz*s;
    T.m[1][1] = c + ny*ny*one_minus_c;
    T.m[1][2] = ny*nz*one_minus_c - nx*s;
    T.m[2][0] = nz*nx*one_minus_c - ny*s;
    T.m[2][1] = nz*ny*one_minus_c + nx*s;
    T.m[2][2] = c + nz*nz*one_minus_c;
    return T;
}

void se3_to_rotvec(Mat4 T, float *rx, float *ry, float *rz, float *tx, float *ty, float *tz) {
    float trace = T.m[0][0] + T.m[1][1] + T.m[2][2];
    float cos_angle = (trace - 1.0f) * 0.5f;
    if (cos_angle > 1.0f) cos_angle = 1.0f;
    if (cos_angle < -1.0f) cos_angle = -1.0f;
    float angle = acosf(cos_angle);
    if (angle < 1e-10f) {
        *rx = *ry = *rz = 0.0f;
    } else {
        float inv2s = 1.0f / (2.0f * sinf(angle));
        *rx = (T.m[2][1] - T.m[1][2]) * inv2s * angle;
        *ry = (T.m[0][2] - T.m[2][0]) * inv2s * angle;
        *rz = (T.m[1][0] - T.m[0][1]) * inv2s * angle;
    }
    *tx = T.m[0][3];
    *ty = T.m[1][3];
    *tz = T.m[2][3];
}