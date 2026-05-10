#include "calibration.h"
#include "../math/camera_math.h"
#include "../math/se3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *find_key_value(const char *json, const char *key) {
    size_t klen = strlen(key);
    const char *p = json;
    while (*p) {
        if (*p == '"') {
            p++;
            if (strncmp(p, key, klen) == 0 && p[klen] == '"') {
                p += klen + 1;
                p = skip_ws(p);
                if (*p == ':') { p++; return skip_ws(p); }
            }
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
        } else {
            p++;
        }
    }
    return NULL;
}

static int read_floats(const char *start, float *out, int max) {
    const char *p = start;
    int count = 0;
    while (*p && count < max) {
        p = skip_ws(p);
        if (*p == '[' || *p == ',') { p++; continue; }
        if (*p == ']') { p++; continue; }
        if (*p == '}') break;
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') {
            char *end;
            float v = strtof(p, &end);
            if (end != p) { out[count++] = v; p = end; continue; }
        }
        p++;
    }
    return count;
}

static void parse_intrinsics(const char *json, const char *key, IntrinsicsData *K) {
    const char *val = find_key_value(json, key);
    if (!val) return;
    float m[9];
    int n = read_floats(val, m, 9);
    if (n >= 9) {
        K->fx = m[0]; K->cx = m[2];
        K->fy = m[4]; K->cy = m[5];
    }
}

static void parse_dist(const char *json, const char *key, float dist[5]) {
    const char *val = find_key_value(json, key);
    if (!val) return;
    read_floats(val, dist, 5);
}

static void parse_mat3(const char *json, const char *key, float M[9]) {
    const char *val = find_key_value(json, key);
    if (!val) return;
    read_floats(val, M, 9);
}

static void parse_vec3(const char *json, const char *key, float v[3]) {
    const char *val = find_key_value(json, key);
    if (!val) return;
    read_floats(val, v, 3);
}

static float parse_float(const char *json, const char *key) {
    const char *val = find_key_value(json, key);
    if (!val) return 0.0f;
    float v;
    int n = read_floats(val, &v, 1);
    return (n == 1) ? v : 0.0f;
}

int calibration_load(CalibrationData *cal, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 65536) { fclose(f); return -1; }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, sz, f);
    fclose(f);
    buf[sz] = '\0';

    memset(cal, 0, sizeof(*cal));
    parse_intrinsics(buf, "K_rgb_left", &cal->K_rgb_left);
    parse_dist(buf, "dist_rgb_left", cal->K_rgb_left.dist);
    parse_intrinsics(buf, "K_rgb_right", &cal->K_rgb_right);
    parse_dist(buf, "dist_rgb_right", cal->K_rgb_right.dist);
    parse_intrinsics(buf, "K_ir", &cal->K_ir);
    parse_dist(buf, "dist_ir", cal->K_ir.dist);

    float R_flat[9];
    parse_mat3(buf, "R_ir_to_headset", R_flat);
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            cal->R_ir[i][j] = R_flat[i*3+j];

    parse_vec3(buf, "t_ir_to_headset", cal->t_ir);
    parse_mat3(buf, "H_ir_to_rgb", cal->H_ir_to_rgb);
    cal->baseline_m = parse_float(buf, "baseline_mm") / 1000.0f;

    free(buf);
    calibration_build_transforms(cal);
    cal->loaded = 1;
    return 0;
}

int calibration_save(const CalibrationData *cal, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\n");
    fprintf(f, "  \"K_rgb_left\": [[%f, 0, %f], [0, %f, %f], [0, 0, 1]],\n",
            cal->K_rgb_left.fx, cal->K_rgb_left.cx, cal->K_rgb_left.fy, cal->K_rgb_left.cy);
    fprintf(f, "  \"dist_rgb_left\": [%f, %f, %f, %f, %f],\n",
            cal->K_rgb_left.dist[0], cal->K_rgb_left.dist[1],
            cal->K_rgb_left.dist[2], cal->K_rgb_left.dist[3], cal->K_rgb_left.dist[4]);
    fprintf(f, "  \"K_rgb_right\": [[%f, 0, %f], [0, %f, %f], [0, 0, 1]],\n",
            cal->K_rgb_right.fx, cal->K_rgb_right.cx, cal->K_rgb_right.fy, cal->K_rgb_right.cy);
    fprintf(f, "  \"dist_rgb_right\": [%f, %f, %f, %f, %f],\n",
            cal->K_rgb_right.dist[0], cal->K_rgb_right.dist[1],
            cal->K_rgb_right.dist[2], cal->K_rgb_right.dist[3], cal->K_rgb_right.dist[4]);
    fprintf(f, "  \"K_ir\": [[%f, 0, %f], [0, %f, %f], [0, 0, 1]],\n",
            cal->K_ir.fx, cal->K_ir.cx, cal->K_ir.fy, cal->K_ir.cy);
    fprintf(f, "  \"dist_ir\": [%f, %f, %f, %f, %f],\n",
            cal->K_ir.dist[0], cal->K_ir.dist[1],
            cal->K_ir.dist[2], cal->K_ir.dist[3], cal->K_ir.dist[4]);
    fprintf(f, "  \"R_ir_to_headset\": [[%f, %f, %f], [%f, %f, %f], [%f, %f, %f]],\n",
            cal->R_ir[0][0], cal->R_ir[0][1], cal->R_ir[0][2],
            cal->R_ir[1][0], cal->R_ir[1][1], cal->R_ir[1][2],
            cal->R_ir[2][0], cal->R_ir[2][1], cal->R_ir[2][2]);
    fprintf(f, "  \"t_ir_to_headset\": [%f, %f, %f],\n",
            cal->t_ir[0], cal->t_ir[1], cal->t_ir[2]);
    fprintf(f, "  \"H_ir_to_rgb\": [[%f, %f, %f], [%f, %f, %f], [%f, %f, %f]],\n",
            cal->H_ir_to_rgb[0], cal->H_ir_to_rgb[1], cal->H_ir_to_rgb[2],
            cal->H_ir_to_rgb[3], cal->H_ir_to_rgb[4], cal->H_ir_to_rgb[5],
            cal->H_ir_to_rgb[6], cal->H_ir_to_rgb[7], cal->H_ir_to_rgb[8]);
    fprintf(f, "  \"baseline_mm\": %f\n", cal->baseline_m * 1000.0f);
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

void calibration_build_transforms(CalibrationData *cal) {
    cal->T_ir_to_headset = se3_from_rt(cal->R_ir, cal->t_ir);
    homography_invert(cal->H_ir_to_rgb, cal->H_inv);
}