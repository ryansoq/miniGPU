// zbuffer — 深度緩衝：per-pixel 深度測試（3D 遮擋的核心）
#pragma once
#include <vector>
struct ZBuffer {
  int w, h; std::vector<float> depth;
  ZBuffer(int w_, int h_) : w(w_), h(h_), depth(w_*h_, 1e30f) {}
  // z 更近（更小）→ 通過並更新，回 true；被擋 → false
  bool testAndSet(int x, int y, float z) {
    int i = y*w+x;
    if (z >= depth[i]) return false;
    depth[i] = z; return true;
  }
};
