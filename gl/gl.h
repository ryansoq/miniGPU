// gl — 迷你圖形 API / 驅動層。
//
// app（teapot.cpp）不直接碰硬體，只呼叫 gl::drawMesh：
//   「用這組 vertex/fragment shader，把這顆 mesh 畫進 framebuffer」。
// gl 負責驅動的活：合成 MVP、上傳 uniform、綁 shader，然後開動 gpu/ 的
// 三段式管線。真正的執行（三段 shader + Z-buffer）在 gpu/ 硬體那層。
//
//   teapot.cpp (app)  →  gl (這層)  →  gpu (硬體)
#pragma once
#include "mesh.h"
#include "math3d.h"
#include "framebuffer.h"
#include "zbuffer.h"
#include <cstdint>
#include <vector>

namespace gl {
// 綁 shader + 上傳 MVP + 開動三段式管線，把 mesh 畫進 fb（含 Z-buffer 遮擋）。
// model/view/proj 由 app 給，gl 合成 MVP 交給 vertex shader。
void drawMesh(const Mesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, const std::vector<uint32_t> &vprog,
              const std::vector<uint32_t> &fprog, Framebuffer &fb, ZBuffer &zb);
}
