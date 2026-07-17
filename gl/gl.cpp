#include "gl.h"
#include "vertex_stage.h"     // gpu：① 頂點階段
#include "rasterizer.h"       // gpu：② 光柵化（內含 ③ 片段 + Z-buffer）

namespace gl {
void drawMesh(const Mesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, const std::vector<uint32_t> &vprog,
              const std::vector<uint32_t> &fprog, Framebuffer &fb, ZBuffer &zb) {
  // 上傳 uniform：把 proj*view*model 合成一顆 MVP 矩陣交給 vertex shader。
  Mat4 mvp = proj * view * model;
  // 開動硬體管線：① vertex shader → ② rasterize(+Z-buffer) → ③ fragment shader。
  auto vout = runVertexStage(mesh, model, mvp, vprog);
  rasterizeMesh(mesh, vout, fprog, fb, zb);
}
}
