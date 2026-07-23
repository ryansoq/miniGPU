#include "gl.h"
#include "vertex_stage.h"     // gpu：① 頂點階段
#include "rasterizer.h"       // gpu：② 光柵化（內含 ③ 片段 + Z-buffer）

namespace gl {
void drawMesh(const Mesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, const std::vector<uint32_t> &vprog,
              const std::vector<uint32_t> &fprog, Framebuffer &fb, ZBuffer &zb) {
  // 上傳 uniform：真引擎版——proj/view/model 三顆分開交給 vertex shader，
  // 由 shader 自己做 proj*(view*(model*pos))。（最簡版見 ToyGPU v1：CPU 先合成 mvp。）
  // 開動硬體管線：① vertex shader → ② rasterize(+Z-buffer) → ③ fragment shader。
  auto vout = runVertexStage(mesh, model, view, proj, vprog);
  rasterizeMesh(mesh, vout, fprog, fb, zb);
}
}
