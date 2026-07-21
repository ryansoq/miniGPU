#!/bin/bash
# miniGPU — 畫龍。跟 build_teapot.sh 是**完全相同**的流程，只換 app + mesh。
#
#   shaders/*.vert|frag  →[compiler]→  build/*.bin  →[gpu 執行]→  dragon.png
#
# 證明重點：龍和茶壺共用同一條編譯鏈、同兩顆 shader、同一顆 shader_core、
# 同一套三段式管線 + Z-buffer。差別只有載入的 OBJ 不同。
#
# 用法：bash build_dragon.sh
set -e
cd "$(dirname "$0")"
LLC=${TOYGPU_LLC:-~/llvm-project/build/bin/llc}
mkdir -p build
say(){ echo -e "\n\033[1;36m▶ $*\033[0m"; }
KEEP='^[[:space:]]+(LDIN|LDUNI|STOUT|LDI|FADD|FSUB|FMUL|FMA|MOV|RET|NOP)'

g++ -std=c++17 -O2 compiler/spirv2llvm.cpp -o build/spirv2llvm

# 跟茶壺一字不差的 shader 編譯鏈（GLSL → SPIR-V → LLVM IR → ToyGPU ISA）
compile_shader(){
  local stage=$1 src=$2 name=$3
  glslangValidator -V -S $stage $src -o build/$name.spv >/dev/null
  build/spirv2llvm build/$name.spv build/$name.ll >/dev/null
  $LLC -mtriple=toygpu build/$name.ll -o build/$name.s.raw
  grep -E "$KEEP" build/$name.s.raw > build/$name.s
  python3 compiler/toyasm.py build/$name.s build/$name.bin >/dev/null
  echo "   ✓ build/$name.bin"
}

say "① 編兩顆 shader（跟茶壺同一條鏈、同兩支 GLSL）"
compile_shader vert shaders/mvp.vert vertex
compile_shader frag shaders/light.frag frag

say "② 編 app + gl + gpu（換成 dragon.cpp，其餘與茶壺相同）"
g++ -std=c++17 -O2 -Igpu -Ihost -Igl \
  dragon.cpp gl/gl.cpp \
  gpu/shader_core.cpp gpu/vertex_stage.cpp gpu/fragment_stage.cpp \
  gpu/rasterizer.cpp gpu/framebuffer.cpp host/mesh.cpp \
  -o build/dragon
echo "   ✓ build/dragon"

say "③ 執行：app → gl → gpu（三段式 + Z-buffer）→ dragon.png"
./build/dragon 0.6 dragon.png
echo -e "\n\033[1;32m完成！→ dragon.png\033[0m"
echo "動畫：把 rotateY 掃一圈算多幀，用 ffmpeg 併成 gif（見 README）"
