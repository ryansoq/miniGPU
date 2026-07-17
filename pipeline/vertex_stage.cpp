#include "vertex_stage.h"
#include "shader_core.h"
std::vector<VOut> runVertexStage(const Mesh &m, const Mat4 &model,
                                 const Mat4 &mvp,
                                 const std::vector<uint32_t> &program) {
  std::vector<VOut> out(m.pos.size());
  // MVP 攤平成 row-major 塞進 U0-U15（vertex shader 讀這個）
  float U[16];
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) U[r*4+c] = mvp.m[r][c];
  for (size_t i = 0; i < m.pos.size(); i++) {
    GpuState st;
    st.I[0] = m.pos[i].x; st.I[1] = m.pos[i].y; st.I[2] = m.pos[i].z;
    for (int k = 0; k < 16; k++) st.U[k] = U[k];
    shader_run(program, st);              // ← vertex shader 在 shader_core 上跑
    float w = st.O[3] == 0 ? 1e-6f : st.O[3];
    out[i].ndc = {st.O[0]/w, st.O[1]/w, st.O[2]/w};   // perspective divide
    // 法向量用 model 轉到世界（固定功能算，不進 shader；打光在 fragment）
    Vec4 n = model * Vec4{m.normal[i].x, m.normal[i].y, m.normal[i].z, 0};
    out[i].nrm = normalize(Vec3{n.x, n.y, n.z});
  }
  return out;
}
