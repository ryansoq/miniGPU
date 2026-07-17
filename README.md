# miniGPU 🫖 — ToyGPU v2：3D 軟體 GPU

從 [ToyGPU v1](https://github.com/ryansoq/ToyGPU)（教學骨架：一個 2D 三角形 +
真 LLVM backend）長出來，補上完整 3D 圖形管線。

```
3D 頂點 → Vertex(MVP 變換) → Rasterizer(+Z-buffer) → Fragment(打光) → PNG
```

## 進度
- [x] v1 base 搬移（gpu/ toygl/ toyasm/ tests/）
- [x] **3D 管線 v1：Utah 茶壺**（math3d + vertex 變換 + Z-buffer + Lambert 打光）✅
- [ ] 貼圖（texture mapping，把閃卡貼上去）
- [ ] 旋轉動畫（canvas real-time loop）
- [ ] 更重的模型（Stanford Bunny/Dragon）+ 加速

## 跑
```bash
g++ -std=c++17 -O2 teapot.cpp gpu3d/renderer3d.cpp -o teapot
./teapot                 # → teapot.png
```

底層的 v1 ISA/LLVM backend 保留在 gpu/ toyasm/（fragment shader 之後可
接回這條真編譯鏈；現階段 3D vertex/lighting 先用 C++ 直算驗證管線）。
