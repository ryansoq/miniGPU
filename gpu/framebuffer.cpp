#include "framebuffer.h"
#include <algorithm>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
static uint8_t to8(float f){ return (uint8_t)std::clamp((int)(f*255+0.5f),0,255); }
void Framebuffer::setPixel(int x, int y, float r, float g, float b){
  uint8_t*p=&rgba[(y*w+x)*4]; p[0]=to8(r);p[1]=to8(g);p[2]=to8(b);p[3]=255;
}
bool Framebuffer::savePNG(const char*path) const {
  return stbi_write_png(path,w,h,4,rgba.data(),w*4)!=0;
}
