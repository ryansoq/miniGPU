// texture — 貼圖取樣（software texture unit 的最小版）。
// 載入圖片、雙線性內插取樣。這對應真 GPU 的 texture unit（只是慢版）。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Texture {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;             // w*h*4
  bool load(const char *path);
  // 雙線性取樣：uv ∈ [0,1]，回傳 rgb（0..1）。
  // 真 texture unit 幫你做的四點混合，這裡手寫一次看清楚。
  void sample(float u, float v, float &r, float &g, float &b) const;
};
