#include "renderer3d.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

bool Framebuffer3D::savePNG(const char *path) const {
  return stbi_write_png(path, w, h, 4, rgba.data(), w * 4) != 0;
}

// ── 最小 OBJ loader ─────────────────────────────────────────────
Mesh loadOBJ(const char *path) {
  Mesh m;
  std::vector<Vec3> vn;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    std::string tag; s >> tag;
    if (tag == "v") { Vec3 v; s >> v.x >> v.y >> v.z; m.pos.push_back(v); }
    else if (tag == "vn") { Vec3 n; s >> n.x >> n.y >> n.z; vn.push_back(n); }
    else if (tag == "f") {
      // 收集這個 face 的頂點索引（支援 v / v//vn / v/vt/vn），fan 三角化
      std::vector<int> vi;
      std::string tok;
      while (s >> tok) {
        int v = std::atoi(tok.c_str());        // 第一個數字就是頂點索引
        if (v < 0) v = (int)m.pos.size() + v + 1;   // 負索引
        vi.push_back(v - 1);                    // OBJ 是 1-based
      }
      for (size_t k = 1; k + 1 < vi.size(); k++) {
        m.idx.push_back(vi[0]); m.idx.push_back(vi[k]); m.idx.push_back(vi[k + 1]);
      }
    }
  }
  // 沒有頂點法向量 → 用相鄰面法向量累加平均（smooth normal）
  m.normal.assign(m.pos.size(), Vec3{0, 0, 0});
  if (vn.size() == m.pos.size()) {
    m.normal = vn;
  } else {
    for (size_t i = 0; i + 2 < m.idx.size(); i += 3) {
      uint32_t a = m.idx[i], b = m.idx[i + 1], c = m.idx[i + 2];
      Vec3 fn = cross(m.pos[b] - m.pos[a], m.pos[c] - m.pos[a]);
      m.normal[a] = m.normal[a] + fn;
      m.normal[b] = m.normal[b] + fn;
      m.normal[c] = m.normal[c] + fn;
    }
    for (auto &n : m.normal) n = normalize(n);
  }
  return m;
}

namespace {
struct Vary { Vec3 ndc; float invw; Vec3 nrm; };   // 每頂點變換後的資料

float edge(float ax, float ay, float bx, float by, float px, float py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
uint8_t to8(float f) { return (uint8_t)std::clamp((int)(f * 255 + 0.5f), 0, 255); }
} // namespace

void drawMesh(const Mesh &mesh, const Mat4 &model, const Mat4 &view,
              const Mat4 &proj, Vec3 lightDir, Framebuffer3D &fb) {
  Mat4 mvp = proj * view * model;
  Vec3 L = normalize(lightDir);

  // ── Vertex 階段：每個頂點跑一次 MVP（這就是 vertex shader 的工作）──
  std::vector<Vary> vs(mesh.pos.size());
  for (size_t i = 0; i < mesh.pos.size(); i++) {
    Vec4 clip = mvp * Vec4{mesh.pos[i].x, mesh.pos[i].y, mesh.pos[i].z, 1};
    float w = clip.w == 0 ? 1e-6f : clip.w;
    vs[i].ndc = {clip.x / w, clip.y / w, clip.z / w};   // perspective divide
    vs[i].invw = 1.0f / w;
    // 法向量用 model 矩陣旋轉到世界（此處 model 只有旋轉，直接乘方向）
    Vec4 n = model * Vec4{mesh.normal[i].x, mesh.normal[i].y, mesh.normal[i].z, 0};
    vs[i].nrm = normalize(Vec3{n.x, n.y, n.z});
  }

  // ── 每三角形 rasterize（+ Z-buffer + Lambert 打光）──
  for (size_t t = 0; t + 2 < mesh.idx.size(); t += 3) {
    const Vary &A = vs[mesh.idx[t]], &B = vs[mesh.idx[t + 1]], &C = vs[mesh.idx[t + 2]];
    // NDC → 螢幕像素
    auto sx = [&](const Vary &v) { return (v.ndc.x * 0.5f + 0.5f) * fb.w; };
    auto sy = [&](const Vary &v) { return (1 - (v.ndc.y * 0.5f + 0.5f)) * fb.h; };
    float x0 = sx(A), y0 = sy(A), x1 = sx(B), y1 = sy(B), x2 = sx(C), y2 = sy(C);

    float area = edge(x0, y0, x1, y1, x2, y2);
    if (area >= 0) continue;               // 背面剔除（順時針=背對，跳過）
    area = -area;

    int minx = std::max(0, (int)std::floor(std::min({x0, x1, x2})));
    int maxx = std::min(fb.w - 1, (int)std::ceil(std::max({x0, x1, x2})));
    int miny = std::max(0, (int)std::floor(std::min({y0, y1, y2})));
    int maxy = std::min(fb.h - 1, (int)std::ceil(std::max({y0, y1, y2})));

    for (int y = miny; y <= maxy; y++)
      for (int x = minx; x <= maxx; x++) {
        float px = x + 0.5f, py = y + 0.5f;
        // 背面已翻，權重用負 area 對應
        float w0 = edge(x1, y1, x2, y2, px, py);
        float w1 = edge(x2, y2, x0, y0, px, py);
        float w2 = edge(x0, y0, x1, y1, px, py);
        if (w0 > 0 || w1 > 0 || w2 > 0) continue;    // 在外面
        float b0 = w0 / -area, b1 = w1 / -area, b2 = w2 / -area;

        // 內插深度（NDC z）
        float z = b0 * A.ndc.z + b1 * B.ndc.z + b2 * C.ndc.z;
        int di = y * fb.w + x;
        if (z >= fb.depth[di]) continue;             // Z-buffer：被擋住
        fb.depth[di] = z;

        // 內插法向量 → Lambert 漫反射
        Vec3 n = normalize(A.nrm * b0 + B.nrm * b1 + C.nrm * b2);
        float diff = std::max(0.0f, dot(n, L));
        float shade = 0.15f + 0.85f * diff;          // ambient + diffuse
        uint8_t *p = &fb.rgba[di * 4];
        p[0] = to8(shade * 0.85f);                   // 暖白瓷器色
        p[1] = to8(shade * 0.82f);
        p[2] = to8(shade * 0.78f);
        p[3] = 255;
      }
  }
}
