// mesh — 幾何資料 + 最小 OBJ 載入
#pragma once
#include "math3d.h"
#include <cstdint>
#include <vector>
struct Mesh {
  std::vector<Vec3> pos, normal;
  std::vector<uint32_t> idx;             // 每 3 個索引 = 一個三角形
};
Mesh loadOBJ(const char *path);
