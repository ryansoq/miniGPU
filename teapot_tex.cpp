// 芙莉蓮茶壺：Utah teapot + 閃卡貼圖
#include "gpu3d/renderer3d.h"
#include <cstdio>
int main(int argc, char** argv) {
  Mesh m = loadOBJ("models/teapot.obj");
  genCylindricalUV(m);                    // 茶壺沒 UV → 圓柱投影自動生
  Texture tex;
  const char* texpath = argc > 1 ? argv[1] : "/home/ymchang/clawd/FRIEREN_Shiny_Card.jpeg";
  if (!tex.load(texpath)) { printf("texture load failed: %s\n", texpath); return 1; }
  printf("teapot %zu tris, texture %dx%d\n", m.idx.size()/3, tex.w, tex.h);

  Framebuffer3D fb(512, 512);
  Mat4 view = lookAt({0, 2.2f, 7.5f}, {0, 1.2f, 0}, {0, 1, 0});
  Mat4 proj = perspective(0.9f, 1.0f, 0.1f, 100.0f);
  Mat4 modelM = rotateY(0.6f);
  drawMesh(m, modelM, view, proj, {0.4f, 0.8f, 0.6f}, fb, &tex);
  fb.savePNG("teapot_frieren.png");
  printf("wrote teapot_frieren.png\n");
  return 0;
}
