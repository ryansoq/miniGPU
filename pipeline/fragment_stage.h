// fragment_stage — 片段階段：每個 pixel 跑 fragment shader ISA（shader_core）。
// I0-I2 = 內插後的法向量 → O0-O2 = RGB。
#pragma once
#include "math3d.h"
#include <cstdint>
#include <vector>
// 跑一次 fragment shader，回傳 rgb。program = fragment ISA。
Vec3 runFragment(const std::vector<uint32_t> &program, Vec3 interpNormal);
