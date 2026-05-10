#ifndef MAT4_H
#define MAT4_H

typedef struct { float m[4][4]; } Mat4;
typedef struct { float v[3]; } Vec3;
typedef struct { float v[4]; } Vec4;

Mat4 mat4_identity(void);
Mat4 mat4_mul(Mat4 a, Mat4 b);
Mat4 mat4_transpose(Mat4 a);
Mat4 mat4_invert(Mat4 a);
Vec3 mat4_apply_point(Mat4 m, Vec3 p);
Vec3 mat4_apply_dir(Mat4 m, Vec3 d);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, float s);
float vec3_dot(Vec3 a, Vec3 b);
float vec3_len(Vec3 v);
Vec3 vec3_normalize(Vec3 v);
Vec3 vec3_cross(Vec3 a, Vec3 b);

#endif