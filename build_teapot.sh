#!/bin/bash
# miniGPU — 一步一步跑完整流程，每一步印出產生的中間檔案。
#
#   shaders/*.vert|frag  →[compiler]→  build/*.bin  →[gpu 執行]→  teapot.png
#
# ① vertex   GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary
# ② fragment 同一條鏈（唯一差別是輸入的 .vert / .frag）
# ③ 編 app + gl + gpu
# ④ 執行：app → gl → gpu（三段式 + Z-buffer）→ PNG
#
# 用法：bash build_teapot.sh
set -e
cd "$(dirname "$0")"
LLC=${TOYGPU_LLC:-~/llvm-project/build/bin/llc}
mkdir -p build
say(){ echo -e "\n\033[1;36m▶ $*\033[0m"; }
KEEP='^[[:space:]]+(LDIN|LDUNI|STOUT|LDI|FADD|FSUB|FMUL|FMA|MOV|RET|NOP)'

# 編一次 SPIR-V→LLVM 的翻譯器（compiler/ 的一部分）
g++ -std=c++17 -O2 compiler/spirv2llvm.cpp -o build/spirv2llvm

# compile_shader <stage:vert|frag> <src> <name>
#   一條鏈：GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary
compile_shader(){
  local stage=$1 src=$2 name=$3
  echo "   [GLSL]      $src"
  glslangValidator -V -S $stage $src -o build/$name.spv >/dev/null
  echo "   [SPIR-V]    build/$name.spv   ($(stat -c%s build/$name.spv) bytes)  ← glslangValidator"
  build/spirv2llvm build/$name.spv build/$name.ll >/dev/null
  echo "   [LLVM IR]   build/$name.ll    ($(stat -c%s build/$name.ll) bytes)  ← spirv2llvm"
  $LLC -mtriple=toygpu build/$name.ll -o build/$name.s.raw
  grep -E "$KEEP" build/$name.s.raw > build/$name.s
  echo "   [ToyGPU asm]build/$name.s     ($(stat -c%s build/$name.s) bytes)  ← llc -mtriple=toygpu"
  python3 compiler/toyasm.py build/$name.s build/$name.bin >/dev/null
  echo "   [ISA bin]   build/$name.bin   ($(stat -c%s build/$name.bin) bytes)  ← toyasm"
}

say "① Vertex shader：GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary"
compile_shader vert shaders/mvp.vert vertex

say "② Fragment shader：GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA binary"
compile_shader frag shaders/light.frag frag

say "③ 編譯 app + gl + gpu"
g++ -std=c++17 -O2 -Igpu -Ihost -Igl \
  teapot.cpp gl/gl.cpp \
  gpu/shader_core.cpp gpu/vertex_stage.cpp gpu/fragment_stage.cpp \
  gpu/rasterizer.cpp gpu/framebuffer.cpp host/mesh.cpp \
  -o build/teapot
echo "   ✓ build/teapot"

say "④ 執行：app → gl → gpu（三段式 + Z-buffer）→ PNG"
./build/teapot
echo -e "\n\033[1;32m完成！→ teapot.png\033[0m"
echo "逐站檢查：spirv-dis build/vertex.spv / cat build/vertex.ll build/vertex.s"
echo "          spirv-dis build/frag.spv   / cat build/frag.ll   build/frag.s"
