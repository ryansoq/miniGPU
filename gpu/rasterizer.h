// rasterizer — 光柵化（固定功能）：三角形 → 覆蓋的 pixel + 內插，
// 每個 pixel 過 zbuffer 深度測試、呼叫 fragment shader、寫 framebuffer。
#pragma once
#include "vertex_stage.h"
#include "framebuffer.h"
#include "zbuffer.h"
#include <cstdint>
#include <vector>
void rasterizeMesh(const Mesh &m, const std::vector<VOut> &vout,
                   const std::vector<uint32_t> &fragProgram,
                   Framebuffer &fb, ZBuffer &zb);
