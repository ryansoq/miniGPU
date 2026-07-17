# miniGPU 🫖 — 一條乾淨的 GPU 流程，從 GLSL 一路跑到茶壺

從 [ToyGPU](https://github.com/ryansoq/ToyGPU)（教學骨架：一個 2D 三角形 +
自寫 LLVM target）長出來，做成一條**完整、簡單、一步一步**的流程：GLSL
著色器經過真的 LLVM 編譯鏈變成 ToyGPU ISA，再由軟體 GPU 跑三段式管線畫出
Utah 茶壺。

```
shaders/*.vert|frag ─[compiler]→ ISA binary ─[gpu 執行]→ teapot.png
       GLSL → SPIR-V → LLVM IR → ToyGPU 組語 → ISA
```

![teapot](docs/teapot.png)

## 三層架構：app → gl → gpu

真實 GPU 就是這樣分層的 —— app 不直接碰硬體，透過圖形 API 間接開動 GPU：

| 層 | 資料夾 | 做什麼 |
|---|---|---|
| **app** | `teapot.cpp` | 載入 mesh + shader、設相機，呼叫 `gl::drawMesh` |
| **gl**（圖形 API/驅動） | `gl/` | 合成 MVP、上傳 uniform、綁 shader，開動管線 |
| **gpu**（硬體） | `gpu/` | 三段式 + Z-buffer，跑在 `shader_core` ISA 核心 |
| **compiler**（著色器工具鏈） | `compiler/` | GLSL→SPIR-V→LLVM IR→ToyGPU ISA |
| **host**（CPU 端） | `host/` | mesh（OBJ 載入）+ math3d（線代/MVP） |

gpu 硬體裡的**三段式管線**（都跑同一個 `shader_core` ISA 核心）：

```
① vertex_stage   MVP 變換頂點        (可程式化，跑 shader)
② rasterizer     三角形 → 覆蓋像素    (固定功能)
③ fragment_stage 每像素打光          (可程式化，跑 shader)
   + zbuffer      深度遮擋            (固定功能)
```

**統一著色器**：vertex 和 fragment 是**同一顆** `shader_core`，只是餵不同的
程式 —— 跟 2006 之後的真 GPU 一樣。兩顆 shader 也走**完全相同**的編譯鏈，
差別只有輸入的 `.vert` / `.frag`。

## 跑

```bash
bash build_teapot.sh
```

一步一步印出每個中間檔案（`build/vertex.spv` → `.ll` → `.s` → `.bin`，
fragment 同理），最後輸出 `teapot.png`。逐站檢查：

```bash
spirv-dis build/vertex.spv     # SPIR-V
cat build/vertex.ll            # LLVM IR（mat4×vec4 被展成 16 讀 12 加）
cat build/vertex.s             # ToyGPU 組語
```

> 需要 `glslangValidator`、`python3`，以及編好的 ToyGPU LLVM target
> （見下方）。

## LLVM Backend Porting

`compiler/backend/` 是一個**真的 LLVM target**（fork llvm-project @ 20.1.8，
加一個 `toygpu` target），不是翻譯腳本。完整過程與踩坑實錄見
**[compiler/backend/BUILD.md](compiler/backend/BUILD.md)** —— 8 個具體問題
（tblgen 型別要求、SelectionDAGTargetInfo segfault、isFPImmLegal、timm vs
imm、STOUT 的 hasSideEffects 防 DCE…）及解法，是「如何 port 一個 LLVM
target」的第一手教材。

```bash
bash compiler/backend/setup-llvm.sh     # clone + Triple patch + symlink + cmake
ninja -C ~/llvm-project/build llc        # 第一次 ~30-60 分鐘
~/llvm-project/build/bin/llc -mtriple=toygpu build/vertex.ll -o out.s
```
