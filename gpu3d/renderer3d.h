// renderer3d — miniGPU 的 3D 管線：vertex 變換 → rasterize + Z-buffer →
// per-pixel 打光。v1 只有 fragment/rasterizer，v2 補上 vertex 階段和深度。
#pragma once
#include "math3d.h"
#include "texture.h"
#include <cstdint>
#include <vector>

struct Mesh {
  std::vector<Vec3> pos;                 // 頂點位置
  std::vector<Vec3> normal;              // 頂點法向量（打光用）
  std::vector<float> u, v;               // 每頂點 UV 紋理座標（貼圖用）
  std::vector<uint32_t> idx;             // 三角形索引（每 3 個一組）
};

// 沒有 UV 的 mesh → 用圓柱投影自動生一組（茶壺這種旋轉體很適合）。
void genCylindricalUV(Mesh &m);

// 讀最小 OBJ（v / vn / f）。face 支援 v、v//vn、v/vt/vn 三種格式。
// 若檔案沒 normal 就自己算面法向量。
Mesh loadOBJ(const char *path);

struct Framebuffer3D {
  int w, h;
  std::vector<uint8_t> rgba;             // w*h*4
  std::vector<float> depth;              // w*h，Z-buffer（初值 +∞）
  Framebuffer3D(int w_, int h_)
      : w(w_), h(h_), rgba(w_ * h_ * 4, 0), depth(w_ * h_, 1e30f) {}
  bool savePNG(const char *path) const;
};

// 畫整個 mesh：MVP 變換每個頂點 → 每三角形 rasterize（Z-buffer 遮擋）
// → Lambert 打光。lightDir 是世界空間的光方向。
// tex 非 nullptr 時做貼圖：fragment 用內插 UV 取樣貼圖，再乘打光。
void drawMesh(const Mesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, Vec3 lightDir, Framebuffer3D &fb,
              const Texture *tex = nullptr);
