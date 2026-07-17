#include "mesh.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
Mesh loadOBJ(const char *path) {
  Mesh m; std::vector<Vec3> vn;
  std::ifstream f(path); std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line); std::string tag; s >> tag;
    if (tag == "v") { Vec3 v; s >> v.x >> v.y >> v.z; m.pos.push_back(v); }
    else if (tag == "vn") { Vec3 n; s >> n.x >> n.y >> n.z; vn.push_back(n); }
    else if (tag == "f") {
      std::vector<int> vi; std::string tok;
      while (s >> tok) { int v = std::atoi(tok.c_str());
        if (v < 0) v = (int)m.pos.size() + v + 1; vi.push_back(v - 1); }
      for (size_t k = 1; k + 1 < vi.size(); k++) {
        m.idx.push_back(vi[0]); m.idx.push_back(vi[k]); m.idx.push_back(vi[k + 1]); }
    }
  }
  m.normal.assign(m.pos.size(), Vec3{0, 0, 0});
  if (vn.size() == m.pos.size()) m.normal = vn;
  else {
    for (size_t i = 0; i + 2 < m.idx.size(); i += 3) {
      uint32_t a = m.idx[i], b = m.idx[i+1], c = m.idx[i+2];
      Vec3 fn = cross(m.pos[b]-m.pos[a], m.pos[c]-m.pos[a]);
      m.normal[a]=m.normal[a]+fn; m.normal[b]=m.normal[b]+fn; m.normal[c]=m.normal[c]+fn;
    }
    for (auto &n : m.normal) n = normalize(n);
  }
  return m;
}
