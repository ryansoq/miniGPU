#include "fragment_stage.h"
#include "shader_core.h"
Vec3 runFragment(const std::vector<uint32_t> &program, Vec3 n) {
  GpuState st;
  st.I[0] = n.x; st.I[1] = n.y; st.I[2] = n.z;
  shader_run(program, st);                // ← fragment shader 在同一顆 shader_core 上跑
  return {st.O[0], st.O[1], st.O[2]};
}
