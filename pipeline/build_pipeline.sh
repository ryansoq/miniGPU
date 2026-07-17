#!/bin/bash
# 一步一步跑完整管線，每一步印出產生的中間檔案。
# 用法：bash pipeline/build_pipeline.sh
set -e
cd "$(dirname "$0")/.."
LLC=${TOYGPU_LLC:-~/llvm-project/build/bin/llc}
say(){ echo -e "\n\033[1;36m▶ $*\033[0m"; }

# vertex/fragment 走「完全相同」的一條鏈，只差輸入的 .vert / .frag。
KEEP='^[[:space:]]+(LDIN|LDUNI|STOUT|LDI|FADD|FSUB|FMUL|FMA|MOV|RET|NOP)'
g++ -std=c++17 -O2 spirv2llvm/main.cpp -o build_spirv2llvm
# compile_shader <stage> <src> <name>：GLSL→SPIR-V→LLVM IR→ToyGPU 組語→ISA binary
compile_shader(){
  local stage=$1 src=$2 name=$3
  echo "   [GLSL]      $src"
  glslangValidator -V -S $stage $src -o ${name}.spv >/dev/null
  echo "   [SPIR-V]    ${name}.spv   ($(stat -c%s ${name}.spv) bytes)  ← glslangValidator"
  ./build_spirv2llvm ${name}.spv ${name}.ll >/dev/null
  echo "   [LLVM IR]   ${name}.ll    ($(stat -c%s ${name}.ll) bytes)  ← spirv2llvm"
  $LLC -mtriple=toygpu ${name}.ll -o ${name}.s.raw
  grep -E "$KEEP" ${name}.s.raw > ${name}.s
  echo "   [ToyGPU asm]${name}.s     ($(stat -c%s ${name}.s) bytes)  ← llc -mtriple=toygpu"
  python3 pipeline/toyasm.py ${name}.s ${name}.bin >/dev/null
  echo "   [ISA bin]   ${name}.bin   ($(stat -c%s ${name}.bin) bytes)  ← toyasm"
}

say "① Vertex shader：GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary"
compile_shader vert shaders3d/mvp.vert pipeline_vertex

say "② Fragment shader：GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary"
compile_shader frag shaders3d/light.frag pipeline_frag

say "③ 編譯管線（vertex/fragment 都跑 shader_core）"
g++ -std=c++17 -O2 teapot_pipeline.cpp \
  pipeline/mesh.cpp pipeline/vertex_stage.cpp pipeline/fragment_stage.cpp \
  pipeline/rasterizer.cpp pipeline/framebuffer.cpp pipeline/shader_core.cpp \
  -o teapot_pipeline
echo "   ✓ teapot_pipeline"

say "④ 執行：3D 頂點 → [vertex ISA] → rasterizer(+zbuffer) → [fragment ISA] → PNG"
./teapot_pipeline
echo -e "\n\033[1;32m完成！→ teapot_pipeline.png\033[0m"
echo "逐站檢查：spirv-dis pipeline_vertex.spv / cat pipeline_vertex.ll pipeline_vertex.s"
echo "          spirv-dis pipeline_frag.spv   / cat pipeline_frag.ll   pipeline_frag.s"
