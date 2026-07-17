// teapot — miniGPU 的 app。
//
// 完整流程：GLSL shader 先被 compiler/ 編成 ToyGPU ISA binary（build/*.bin），
// 這支 app 載入 mesh + 兩顆 shader binary，設好相機，呼叫 gl::drawMesh，
// gl 開動 gpu 的三段式管線畫出茶壺 → teapot.png。
//
// app 只碰 gl，不碰硬體。
#include "gl.h"
#include "mesh.h"
#include "framebuffer.h"
#include "zbuffer.h"
#include <cstdio>
#include <fstream>

static std::vector<uint32_t> loadBin(const char *p) {
  std::ifstream f(p, std::ios::binary);
  std::vector<uint32_t> w; uint32_t x;
  while (f.read((char *)&x, 4)) w.push_back(x);
  return w;
}

int main(int argc, char **argv) {
  Mesh m = loadOBJ("models/teapot.obj");
  auto vprog = loadBin(argc > 1 ? argv[1] : "build/vertex.bin");
  auto fprog = loadBin(argc > 2 ? argv[2] : "build/frag.bin");
  printf("mesh %zu tris | vertex ISA %zu words | fragment ISA %zu words\n",
         m.idx.size() / 3, vprog.size(), fprog.size());

  Framebuffer fb(512, 512);
  ZBuffer zb(512, 512);
  // 相機 + 物件姿態（host 端算好，交給 gl 上傳）
  Mat4 model = rotateY(0.6f);
  Mat4 view  = lookAt({0, 2.2f, 7.5f}, {0, 1.2f, 0}, {0, 1, 0});
  Mat4 proj  = perspective(0.9f, 1.0f, 0.1f, 100.0f);

  gl::drawMesh(m, model, view, proj, vprog, fprog, fb, zb);

  fb.savePNG("teapot.png");
  printf("wrote teapot.png\n");
  return 0;
}
