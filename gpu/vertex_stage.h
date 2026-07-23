// vertex_stage — 頂點階段：每個頂點跑 vertex shader ISA（shader_core）。
// U0-U15 = MVP 矩陣，I0-I2 = 頂點位置 → O0-O3 = clip 座標。
// program 跟 fragment 一樣是 GLSL→SPIR-V→LLVM→ToyGPU ISA 編出來的
// （shaders/mvp.vert），兩段共用同一條編譯鏈、同一個 shader_core。
#pragma once
#include "math3d.h"
#include "mesh.h"
#include <cstdint>
#include <vector>
struct VOut { Vec3 ndc; Vec3 nrm; };   // perspective divide 後的 NDC + 世界法向量
// program = vertex shader 的 ToyGPU ISA。跑每個頂點，回傳變換後的頂點。
// 真引擎版：proj/view/model 三顆分開餵進 U（16×3），shader 自己做
// proj*(view*(model*pos))。（最簡版見 ToyGPU v1（先合成一顆 mvp）。）
std::vector<VOut> runVertexStage(const Mesh &m, const Mat4 &model,
                                 const Mat4 &view, const Mat4 &proj,
                                 const std::vector<uint32_t> &program);
