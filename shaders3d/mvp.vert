#version 450
// Vertex shader：把 3D 頂點用 MVP 矩陣變換到 clip space。
// mvp 是 uniform（driver 每幀填好 proj*view*model），pos 是頂點位置。
// gl_Position 交給固定功能 rasterizer 做 perspective divide (÷w)。
//
// 這支會走跟 fragment 完全一樣的鏈：
//   GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary
// mat4×vec4 由 spirv2llvm 展成 16 個 @toygpu.uniform 讀 + 乘加，
// uniform slot 4r+c 對應 vertex_stage.cpp 攤平的 U[4r+c]=mvp.m[r][c]。
layout(location = 0) in vec3 pos;
layout(binding = 0) uniform UBO { mat4 mvp; };
void main() {
  gl_Position = mvp * vec4(pos, 1.0);
}
