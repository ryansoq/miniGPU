// miniGPU v2 里程碑：畫出 Utah 茶壺（3D 管線 + Z-buffer + 打光）
#include "gpu3d/renderer3d.h"
#include <cstdio>

int main(int argc, char** argv) {
  const char* model = argc > 1 ? argv[1] : "models/teapot.obj";
  Mesh m = loadOBJ(model);
  printf("loaded %s: %zu verts, %zu tris\n", model, m.pos.size(), m.idx.size()/3);

  Framebuffer3D fb(512, 512);
  // 相機擺遠一點看整個茶壺（teapot 大概 y∈[0,3.15], 寬 ~6）
  Mat4 view = lookAt({0, 2.2f, 7.5f}, {0, 1.2f, 0}, {0, 1, 0});
  Mat4 proj = perspective(0.9f, 1.0f, 0.1f, 100.0f);
  Mat4 modelM = rotateY(0.6f);            // 轉個角度好看
  drawMesh(m, modelM, view, proj, {0.4f, 0.8f, 0.6f}, fb);

  const char* out = argc > 2 ? argv[2] : "teapot.png";
  if (!fb.savePNG(out)) return 1;
  printf("wrote %s\n", out);
  return 0;
}
