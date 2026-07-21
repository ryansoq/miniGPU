// dragon — 跟 teapot 一模一樣的 miniGPU 流程，只是換一顆 mesh。
//
// 重點：用的是**跟茶壺完全相同**的兩顆 shader（build/vertex.bin +
// build/frag.bin）——vertex 只做 MVP、fragment 只做打光，跟畫什麼模型無關，
// 所以換模型不必重編 shader。app 只碰 gl，不碰硬體。
//
// 用法：./build/dragon [rotateY 弧度] [輸出.png]
#include "gl.h"
#include "mesh.h"
#include "framebuffer.h"
#include "zbuffer.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

static std::vector<uint32_t> loadBin(const char *p) {
  std::ifstream f(p, std::ios::binary);
  std::vector<uint32_t> w; uint32_t x;
  while (f.read((char *)&x, 4)) w.push_back(x);
  return w;
}

int main(int argc, char **argv) {
  Mesh m = loadOBJ("models/dragon.obj");
  auto vprog = loadBin("build/vertex.bin");   // 茶壺編好的同一顆 vertex shader
  auto fprog = loadBin("build/frag.bin");     // 茶壺編好的同一顆 fragment shader
  printf("dragon: %zu tris | vertex ISA %zu | fragment ISA %zu words\n",
         m.idx.size() / 3, vprog.size(), fprog.size());

  int W = 640, H = 640;
  Framebuffer fb(W, H);
  ZBuffer zb(W, H);

  float ry = argc > 1 ? atof(argv[1]) : 0.6f;
  Mat4 model = rotateY(ry);
  Mat4 view  = lookAt({0, 0.3f, 5.2f}, {0, 0.0f, 0}, {0, 1, 0});
  Mat4 proj  = perspective(0.8f, 1.0f, 0.1f, 100.0f);

  gl::drawMesh(m, model, view, proj, vprog, fprog, fb, zb);

  const char *out = argc > 2 ? argv[2] : "dragon.png";
  fb.savePNG(out);
  printf("wrote %s\n", out);
  return 0;
}
