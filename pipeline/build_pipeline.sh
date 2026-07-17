#!/bin/bash
# 一步一步跑完整管線，每一步印出產生的中間檔案。
# 用法：bash pipeline/build_pipeline.sh
set -e
cd "$(dirname "$0")/.."
LLC=${TOYGPU_LLC:-~/llvm-project/build/bin/llc}
say(){ echo -e "\n\033[1;36m▶ $*\033[0m"; }

say "① Vertex shader（ToyGPU 組語）→ ISA binary"
echo "   shaders/vertex.s  ——手寫的 MVP 矩陣乘法"
python3 pipeline/toyasm.py shaders/vertex.s pipeline_vertex.bin
echo "   ✓ pipeline_vertex.bin  ($(stat -c%s pipeline_vertex.bin) bytes)"

say "② Fragment shader：GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary"
echo "   [GLSL]      shaders3d/light.frag"
glslangValidator -V -S frag shaders3d/light.frag -o pipeline_frag.spv >/dev/null
echo "   [SPIR-V]    pipeline_frag.spv   ($(stat -c%s pipeline_frag.spv) bytes)  ← glslangValidator"
g++ -std=c++17 -O2 spirv2llvm/main.cpp -o build_spirv2llvm
./build_spirv2llvm pipeline_frag.spv pipeline_frag.ll >/dev/null
echo "   [LLVM IR]   pipeline_frag.ll    ($(stat -c%s pipeline_frag.ll) bytes)  ← spirv2llvm"
$LLC -mtriple=toygpu pipeline_frag.ll -o pipeline_frag.s.raw
grep -E '^[[:space:]]+(LDIN|LDUNI|STOUT|LDI|FADD|FSUB|FMUL|FMA|MOV|RET|NOP)' pipeline_frag.s.raw > pipeline_frag.s
echo "   [ToyGPU asm]pipeline_frag.s     ($(stat -c%s pipeline_frag.s) bytes)  ← llc -mtriple=toygpu"
python3 pipeline/toyasm.py pipeline_frag.s pipeline_frag.bin >/dev/null
echo "   [ISA bin]   pipeline_frag.bin   ($(stat -c%s pipeline_frag.bin) bytes)  ← toyasm"

say "③ 編譯管線（vertex/fragment 都跑 shader_core）"
g++ -std=c++17 -O2 teapot_pipeline.cpp \
  pipeline/mesh.cpp pipeline/vertex_stage.cpp pipeline/fragment_stage.cpp \
  pipeline/rasterizer.cpp pipeline/framebuffer.cpp pipeline/shader_core.cpp \
  -o teapot_pipeline
echo "   ✓ teapot_pipeline"

say "④ 執行：3D 頂點 → [vertex ISA] → rasterizer(+zbuffer) → [fragment ISA] → PNG"
./teapot_pipeline
echo -e "\n\033[1;32m完成！→ teapot_pipeline.png\033[0m"
echo "逐站檢查：cat pipeline_frag.ll / pipeline_frag.s / spirv-dis pipeline_frag.spv"
