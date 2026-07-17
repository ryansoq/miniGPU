#include "texture.h"
#include <algorithm>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool Texture::load(const char *path) {
  int n;
  uint8_t *d = stbi_load(path, &w, &h, &n, 4);   // 強制 RGBA
  if (!d) return false;
  rgba.assign(d, d + (size_t)w * h * 4);
  stbi_image_free(d);
  return true;
}

void Texture::sample(float u, float v, float &r, float &g, float &b) const {
  if (w == 0) { r = g = b = 1; return; }
  // wrap（repeat）+ 翻 v（圖片 row0 在上，貼圖慣例 v=0 在下）
  u = u - std::floor(u);
  v = 1.0f - (v - std::floor(v));
  float fx = u * (w - 1), fy = v * (h - 1);
  int x0 = (int)fx, y0 = (int)fy;
  int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
  float tx = fx - x0, ty = fy - y0;
  auto px = [&](int x, int y, int c) {
    return rgba[((size_t)y * w + x) * 4 + c] / 255.0f;
  };
  // 雙線性：4 個 texel 依距離混合
  for (int c = 0; c < 3; c++) {
    float top = px(x0, y0, c) * (1 - tx) + px(x1, y0, c) * tx;
    float bot = px(x0, y1, c) * (1 - tx) + px(x1, y1, c) * tx;
    float val = top * (1 - ty) + bot * ty;
    (c == 0 ? r : c == 1 ? g : b) = val;
  }
}
