// 茶壺 —— fragment shader 走完整 GLSL→SPIR-V→LLVM→ToyGPU ISA 編譯鏈
#include "gpu3d/renderer3d.h"
#include <cstdio>
#include <fstream>
static std::vector<uint32_t> loadBin(const char* p){
  std::ifstream f(p,std::ios::binary); std::vector<uint32_t> w; uint32_t x;
  while(f.read((char*)&x,4)) w.push_back(x); return w;
}
int main(int argc,char**argv){
  Mesh m = loadOBJ("models/teapot.obj");
  auto prog = loadBin(argc>1?argv[1]:"build_light.bin");
  printf("teapot %zu tris, shader %zu words (real ToyGPU ISA)\n", m.idx.size()/3, prog.size());
  Framebuffer3D fb(512,512);
  Mat4 view = lookAt({0,2.2f,7.5f},{0,1.2f,0},{0,1,0});
  Mat4 proj = perspective(0.9f,1.0f,0.1f,100.0f);
  drawMeshShaderISA(m, rotateY(0.6f), view, proj, prog, fb);
  fb.savePNG("teapot_spirv.png");
  printf("wrote teapot_spirv.png\n");
  return 0;
}
