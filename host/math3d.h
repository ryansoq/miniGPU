// math3d — 最小 3D 線代（vec3 / vec4 / mat4）。
// 只放渲染管線用得到的：矩陣乘法、MVP 三件套、法向量運算。
#pragma once
#include <cmath>

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalize(Vec3 v) {
  float l = std::sqrt(dot(v, v));
  return l > 1e-8f ? v * (1.0f / l) : v;
}

struct Vec4 { float x = 0, y = 0, z = 0, w = 1; };

// row-major 4x4；m[行][列]
struct Mat4 {
  float m[4][4] = {};
  static Mat4 identity() {
    Mat4 r;
    for (int i = 0; i < 4; i++) r.m[i][i] = 1;
    return r;
  }
};

// 矩陣 × 矩陣
inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {
  Mat4 r;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += a.m[i][k] * b.m[k][j];
      r.m[i][j] = s;
    }
  return r;
}
// 矩陣 × 向量（點：w=1）
inline Vec4 operator*(const Mat4 &a, Vec4 v) {
  return {a.m[0][0]*v.x + a.m[0][1]*v.y + a.m[0][2]*v.z + a.m[0][3]*v.w,
          a.m[1][0]*v.x + a.m[1][1]*v.y + a.m[1][2]*v.z + a.m[1][3]*v.w,
          a.m[2][0]*v.x + a.m[2][1]*v.y + a.m[2][2]*v.z + a.m[2][3]*v.w,
          a.m[3][0]*v.x + a.m[3][1]*v.y + a.m[3][2]*v.z + a.m[3][3]*v.w};
}

// ── MVP 三件套 ────────────────────────────────────────────────
// Model：把物件擺進世界（這裡只做繞 Y 軸旋轉，之後可加平移/縮放）
inline Mat4 rotateY(float a) {
  Mat4 r = Mat4::identity();
  r.m[0][0] = std::cos(a);  r.m[0][2] = std::sin(a);
  r.m[2][0] = -std::sin(a); r.m[2][2] = std::cos(a);
  return r;
}
inline Mat4 rotateX(float a) {
  Mat4 r = Mat4::identity();
  r.m[1][1] = std::cos(a);  r.m[1][2] = -std::sin(a);
  r.m[2][1] = std::sin(a);  r.m[2][2] = std::cos(a);
  return r;
}
inline Mat4 translate(Vec3 t) {
  Mat4 r = Mat4::identity();
  r.m[0][3] = t.x; r.m[1][3] = t.y; r.m[2][3] = t.z;
  return r;
}
// View：把相機擺到 eye、看向 center（LookAt）
inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
  Vec3 f = normalize(center - eye);      // 前
  Vec3 s = normalize(cross(f, up));      // 右
  Vec3 u = cross(s, f);                  // 上
  Mat4 r = Mat4::identity();
  r.m[0][0] = s.x; r.m[0][1] = s.y; r.m[0][2] = s.z; r.m[0][3] = -dot(s, eye);
  r.m[1][0] = u.x; r.m[1][1] = u.y; r.m[1][2] = u.z; r.m[1][3] = -dot(u, eye);
  r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = dot(f, eye);
  return r;
}
// Projection：透視投影（把 z 放進 w，硬體/rasterizer 之後做 /w = 近大遠小）
inline Mat4 perspective(float fovyRad, float aspect, float znear, float zfar) {
  float t = 1.0f / std::tan(fovyRad * 0.5f);
  Mat4 r;
  r.m[0][0] = t / aspect;
  r.m[1][1] = t;
  r.m[2][2] = (zfar + znear) / (znear - zfar);
  r.m[2][3] = (2 * zfar * znear) / (znear - zfar);
  r.m[3][2] = -1;                        // 這一格把 -z 送進輸出 w
  return r;
}
