#include "rasterizer.h"
#include "fragment_stage.h"
#include <algorithm>
#include <cmath>
static float edge(float ax,float ay,float bx,float by,float px,float py){
  return (bx-ax)*(py-ay)-(by-ay)*(px-ax);
}
void rasterizeMesh(const Mesh &m, const std::vector<VOut> &vout,
                   const std::vector<uint32_t> &fragProgram,
                   Framebuffer &fb, ZBuffer &zb) {
  for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
    const VOut &A = vout[m.idx[t]], &B = vout[m.idx[t+1]], &C = vout[m.idx[t+2]];
    auto sx=[&](const VOut&v){ return (v.ndc.x*0.5f+0.5f)*fb.w; };
    auto sy=[&](const VOut&v){ return (1-(v.ndc.y*0.5f+0.5f))*fb.h; };
    float x0=sx(A),y0=sy(A),x1=sx(B),y1=sy(B),x2=sx(C),y2=sy(C);
    float area=edge(x0,y0,x1,y1,x2,y2);
    if (area>=0) continue;                 // 背面剔除
    area=-area;                            // 翻正（barycentric 正號）
    int minx=std::max(0,(int)std::floor(std::min({x0,x1,x2})));
    int maxx=std::min(fb.w-1,(int)std::ceil(std::max({x0,x1,x2})));
    int miny=std::max(0,(int)std::floor(std::min({y0,y1,y2})));
    int maxy=std::min(fb.h-1,(int)std::ceil(std::max({y0,y1,y2})));
    for (int y=miny;y<=maxy;y++) for (int x=minx;x<=maxx;x++){
      float px=x+0.5f,py=y+0.5f;
      float w0=edge(x1,y1,x2,y2,px,py),w1=edge(x2,y2,x0,y0,px,py),w2=edge(x0,y0,x1,y1,px,py);
      if (w0>0||w1>0||w2>0) continue;      // 在三角形外
      float b0=w0/-area,b1=w1/-area,b2=w2/-area;
      float z=b0*A.ndc.z+b1*B.ndc.z+b2*C.ndc.z;
      if (!zb.testAndSet(x,y,z)) continue; // ← zbuffer 深度測試
      Vec3 n=normalize(A.nrm*b0+B.nrm*b1+C.nrm*b2);
      Vec3 col=runFragment(fragProgram, n);// ← fragment shader（shader_core）
      fb.setPixel(x,y,col.x,col.y,col.z);
    }
  }
}
