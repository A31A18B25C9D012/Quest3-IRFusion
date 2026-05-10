#ifndef SE3_H
#define SE3_H

#include "mat4.h"

Mat4 se3_from_rt(float R[3][3], float t[3]);
Mat4 se3_invert(Mat4 T);
Mat4 se3_compose(Mat4 T1, Mat4 T2);
Vec3 se3_apply(Mat4 T, Vec3 p);
void se3_to_rt(Mat4 T, float R_out[3][3], float t_out[3]);
Mat4 se3_from_rotvec(float rx, float ry, float rz, float tx, float ty, float tz);
void se3_to_rotvec(Mat4 T, float *rx, float *ry, float *rz, float *tx, float *ty, float *tz);

#endif