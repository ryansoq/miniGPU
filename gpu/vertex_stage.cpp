#include "vertex_stage.h"
#include "shader_core.h"
std::vector<VOut> runVertexStage(const Mesh &m, const Mat4 &model,
                                 const Mat4 &view, const Mat4 &proj,
                                 const std::vector<uint32_t> &program) {
  std::vector<VOut> out(m.pos.size());
  // 真引擎版：三顆矩陣分開攤平塞進 U（UBO 順序 = proj, view, model）：
  //   U0-15 = proj、U16-31 = view、U32-47 = model（各自 row-major）
  // vertex shader 讀這三顆、自己做 proj*(view*(model*pos))。
  float U[48];
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
    U[r*4 + c]      = proj.m[r][c];   // member 0
    U[16 + r*4 + c] = view.m[r][c];   // member 1
    U[32 + r*4 + c] = model.m[r][c];  // member 2
  }
  for (size_t i = 0; i < m.pos.size(); i++) {
    GpuState st;
    st.I[0] = m.pos[i].x; st.I[1] = m.pos[i].y; st.I[2] = m.pos[i].z;
    for (int k = 0; k < 48; k++) st.U[k] = U[k];
    shader_run(program, st);              // ← vertex shader 在 shader_core 上跑
    float w = st.O[3] == 0 ? 1e-6f : st.O[3];
    out[i].ndc = {st.O[0]/w, st.O[1]/w, st.O[2]/w};   // perspective divide
    // 法向量用 model 轉到世界（固定功能算，不進 shader；打光在 fragment）
    Vec4 n = model * Vec4{m.normal[i].x, m.normal[i].y, m.normal[i].z, 0};
    out[i].nrm = normalize(Vec3{n.x, n.y, n.z});
  }
  return out;
}
