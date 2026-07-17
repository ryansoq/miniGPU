// miniGPU 三段式管線 demo：vertex/fragment 都跑 shader_core ISA，
// rasterizer + zbuffer 是固定功能，每階段一個 .cpp。
#include "pipeline/mesh.h"
#include "pipeline/vertex_stage.h"
#include "pipeline/rasterizer.h"
#include "pipeline/framebuffer.h"
#include "pipeline/zbuffer.h"
#include <cstdio>
#include <fstream>
static std::vector<uint32_t> loadBin(const char*p){
  std::ifstream f(p,std::ios::binary); std::vector<uint32_t> w; uint32_t x;
  while(f.read((char*)&x,4)){ w.push_back(x); } return w;
}
int main(int argc,char**argv){
  Mesh m = loadOBJ("models/teapot.obj");
  auto vprog = loadBin(argc>1?argv[1]:"pipeline_vertex.bin");
  auto fprog = loadBin(argc>2?argv[2]:"pipeline_frag.bin");
  printf("mesh %zu tris | vertex ISA %zu words | fragment ISA %zu words\n",
         m.idx.size()/3, vprog.size(), fprog.size());
  Framebuffer fb(512,512); ZBuffer zb(512,512);
  Mat4 model=rotateY(0.6f);
  Mat4 view=lookAt({0,2.2f,7.5f},{0,1.2f,0},{0,1,0});
  Mat4 proj=perspective(0.9f,1.0f,0.1f,100.0f);
  Mat4 mvp=proj*view*model;
  // 三段式：vertex → rasterizer(+zbuffer) → fragment，兩個 shader 都跑 shader_core
  auto vout = runVertexStage(m, model, mvp, vprog);
  rasterizeMesh(m, vout, fprog, fb, zb);
  fb.savePNG("teapot_pipeline.png");
  printf("wrote teapot_pipeline.png\n");
  return 0;
}
