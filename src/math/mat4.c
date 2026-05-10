#include "mat4.h"
#include <math.h>
#include <string.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

Mat4 mat4_identity(void) {
    Mat4 m;
    memset(&m, 0, sizeof(m));
    m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0f;
    return m;
}

#ifdef __ARM_NEON
Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 result;
    float32x4_t b0 = vld1q_f32(b.m[0]);
    float32x4_t b1 = vld1q_f32(b.m[1]);
    float32x4_t b2 = vld1q_f32(b.m[2]);
    float32x4_t b3 = vld1q_f32(b.m[3]);
    int i;
    for (i = 0; i < 4; i++) {
        float32x4_t row = vmulq_n_f32(b0, a.m[i][0]);
        row = vmlaq_n_f32(row, b1, a.m[i][1]);
        row = vmlaq_n_f32(row, b2, a.m[i][2]);
        row = vmlaq_n_f32(row, b3, a.m[i][3]);
        vst1q_f32(result.m[i], row);
    }
    return result;
}
#else
Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 result;
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float s = 0.0f;
            for (k = 0; k < 4; k++)
                s += a.m[i][k] * b.m[k][j];
            result.m[i][j] = s;
        }
    }
    return result;
}
#endif

Mat4 mat4_transpose(Mat4 a) {
    Mat4 result;
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            result.m[i][j] = a.m[j][i];
    return result;
}

static float cofactor(const float m[4][4], int r, int c) {
    float sub[3][3];
    int si = 0, i, j, sj;
    for (i = 0; i < 4; i++) {
        if (i == r) continue;
        sj = 0;
        for (j = 0; j < 4; j++) {
            if (j == c) continue;
            sub[si][sj++] = m[i][j];
        }
        si++;
    }
    float d = sub[0][0] * (sub[1][1]*sub[2][2] - sub[1][2]*sub[2][1])
            - sub[0][1] * (sub[1][0]*sub[2][2] - sub[1][2]*sub[2][0])
            + sub[0][2] * (sub[1][0]*sub[2][1] - sub[1][1]*sub[2][0]);
    return ((r + c) % 2 == 0) ? d : -d;
}

Mat4 mat4_invert(Mat4 a) {
    float det = 0.0f;
    int j;
    for (j = 0; j < 4; j++)
        det += a.m[0][j] * cofactor(a.m, 0, j);
    if (det > -1e-8f && det < 1e-8f)
        return mat4_identity();
    float inv_det = 1.0f / det;
    Mat4 result;
    int i;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            result.m[i][j] = cofactor(a.m, j, i) * inv_det;
    return result;
}

Vec3 mat4_apply_point(Mat4 m, Vec3 p) {
    Vec3 result;
    float w;
    result.v[0] = m.m[0][0]*p.v[0] + m.m[0][1]*p.v[1] + m.m[0][2]*p.v[2] + m.m[0][3];
    result.v[1] = m.m[1][0]*p.v[0] + m.m[1][1]*p.v[1] + m.m[1][2]*p.v[2] + m.m[1][3];
    result.v[2] = m.m[2][0]*p.v[0] + m.m[2][1]*p.v[1] + m.m[2][2]*p.v[2] + m.m[2][3];
    w           = m.m[3][0]*p.v[0] + m.m[3][1]*p.v[1] + m.m[3][2]*p.v[2] + m.m[3][3];
    if (w != 0.0f && w != 1.0f) {
        float iw = 1.0f / w;
        result.v[0] *= iw;
        result.v[1] *= iw;
        result.v[2] *= iw;
    }
    return result;
}

Vec3 mat4_apply_dir(Mat4 m, Vec3 d) {
    Vec3 result;
    result.v[0] = m.m[0][0]*d.v[0] + m.m[0][1]*d.v[1] + m.m[0][2]*d.v[2];
    result.v[1] = m.m[1][0]*d.v[0] + m.m[1][1]*d.v[1] + m.m[1][2]*d.v[2];
    result.v[2] = m.m[2][0]*d.v[0] + m.m[2][1]*d.v[1] + m.m[2][2]*d.v[2];
    return result;
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    Vec3 r = {{a.v[0]+b.v[0], a.v[1]+b.v[1], a.v[2]+b.v[2]}};
    return r;
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    Vec3 r = {{a.v[0]-b.v[0], a.v[1]-b.v[1], a.v[2]-b.v[2]}};
    return r;
}

Vec3 vec3_scale(Vec3 v, float s) {
    Vec3 r = {{v.v[0]*s, v.v[1]*s, v.v[2]*s}};
    return r;
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.v[0]*b.v[0] + a.v[1]*b.v[1] + a.v[2]*b.v[2];
}

float vec3_len(Vec3 v) {
    return sqrtf(v.v[0]*v.v[0] + v.v[1]*v.v[1] + v.v[2]*v.v[2]);
}

Vec3 vec3_normalize(Vec3 v) {
    float l = vec3_len(v);
    if (l < 1e-12f) {
        Vec3 z = {{0.0f, 0.0f, 0.0f}};
        return z;
    }
    return vec3_scale(v, 1.0f / l);
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    Vec3 r = {{
        a.v[1]*b.v[2] - a.v[2]*b.v[1],
        a.v[2]*b.v[0] - a.v[0]*b.v[2],
        a.v[0]*b.v[1] - a.v[1]*b.v[0]
    }};
    return r;
}