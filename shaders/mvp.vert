#version 450
// Vertex shader（真引擎版，三專案共用）：proj / view / model 三顆矩陣分開由 driver
// 上傳（一個 UBO 裝三顆），shader 自己做 proj*(view*(model*pos))。
// 最簡版見 ToyGPU v1（先合成 mvp、shader 只做 mvp*pos）。
//
// ★ 刻意寫成右結合 proj*(view*(model*vec4)) → 三次「矩陣 × 向量」
//   (OpMatrixTimesVector)，剛好是我們 spirv2llvm 支援的運算。
//   若寫成 proj*view*model*vec4 會先做「矩陣 × 矩陣」(OpMatrixTimesMatrix)，
//   那是我們還沒實作的 OP。
layout(location = 0) in vec3 pos;
layout(binding = 0) uniform UBO { mat4 proj; mat4 view; mat4 model; };
void main() {
  gl_Position = proj * (view * (model * vec4(pos, 1.0)));
}
