// framebuffer — 顏色緩衝（RGBA8）+ PNG 輸出
#pragma once
#include <cstdint>
#include <vector>
struct Framebuffer {
  int w, h; std::vector<uint8_t> rgba;
  Framebuffer(int w_, int h_) : w(w_), h(h_), rgba(w_*h_*4, 0) {}
  void setPixel(int x, int y, float r, float g, float b);
  bool savePNG(const char *path) const;
};
