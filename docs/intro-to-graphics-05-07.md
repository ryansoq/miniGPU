# Intro to Graphics 重點筆記（05–07）

> 來源：Cem Yuksel（University of Utah）《Intro to Graphics》課程
> - 05 — 2D Transformations
> - 06 — 3D Transformations
> - 07 — GPU Pipeline
>
> 這三集剛好串成一條線：先學「變換是什麼」（2D→3D 矩陣），
> 再看「這些變換在真硬體管線的哪一段執行」（GPU pipeline）。
> 尾端有一節把每個概念對回我們自己的 miniGPU 程式碼。

---

## 05 — 2D Transformations

### 核心概念：變換 = 用矩陣搬動座標

一個點 `p = (x, y)`，變換就是把它映射到新位置 `p' = M·p`。
圖學裡幾乎所有動作（移動、旋轉、縮放、投影）都能寫成「矩陣乘向量」。
好處：**可組合**（一串變換乘成一顆矩陣）、**硬體友善**（GPU 最會做矩陣乘）。

### 線性變換（Linear）— 2×2 矩陣搞定

原點固定不動的變換，都能寫成 2×2 矩陣：

- **縮放 Scale**
  ```
  | sx  0 |
  |  0 sy |
  ```
- **旋轉 Rotation（逆時針 θ）**
  ```
  | cosθ  -sinθ |
  | sinθ   cosθ |
  ```
- **切變 Shear**
  ```
  | 1  k |      | 1  0 |
  | 0  1 |  或  | k  1 |
  ```

線性變換的性質：直線還是直線、原點不動、可疊加。

### 問題：平移（Translation）不是線性的

平移 `p' = p + t` 沒辦法用 2×2 矩陣表達（原點會動）。
如果只用 2×2，就得「矩陣乘 + 向量加」兩步，很難串起來。

### 解法：齊次座標（Homogeneous Coordinates）

把 2D 點升到 3 維：`(x, y)` → `(x, y, 1)`，用 **3×3 矩陣**：

- **平移 Translation**
  ```
  | 1  0  tx |
  | 0  1  ty |
  | 0  0   1 |
  ```
- 線性變換照樣塞進左上 2×2，最後一列 `0 0 1`。

**齊次座標的威力**：平移也變成「純矩陣乘法」了 → 所有變換統一成矩陣，
可以無限串接成一顆矩陣，再一次乘上去。這是整個圖學管線的地基。

> 直覺：第三個分量 `w=1` 是個「錨」，讓平移量 `tx, ty` 有地方掛。
> 點的 `w=1`，方向向量的 `w=0`（方向不該被平移影響）——這個區分在 3D 打光算法向量時很關鍵。

### 組合變換：順序很重要（矩陣不可交換）

`M = M3 · M2 · M1` 代表「先做 M1，再 M2，再 M3」（**右邊先作用**）。

`A·B ≠ B·A`：先旋轉再平移 ≠ 先平移再旋轉，畫出來完全不同。

**經典題：繞任意點 c 旋轉**（不是繞原點）：
```
M = T(c) · R(θ) · T(-c)
```
讀法（右到左）：先把 c 搬到原點 → 繞原點轉 → 再搬回去。

### 逆變換
每個變換有反矩陣：`Scale⁻¹` 是倒數縮放、`Rotation⁻¹ = Rᵀ`（旋轉矩陣正交，
反矩陣就是轉置）、`Translation⁻¹` 是反向平移。

---

## 06 — 3D Transformations

### 從 2D 到 3D：升一維，用 4×4 矩陣

- 點：`(x, y, z)` → 齊次 `(x, y, z, w)`，通常 `w=1`
- 所有變換變成 **4×4 矩陣**
- 縮放、平移直接類推（多一個 z 分量）

### 3D 旋轉：繞三個軸各一顆矩陣

- **繞 X 軸**
  ```
  | 1    0      0   |
  | 0  cosθ  -sinθ  |
  | 0  sinθ   cosθ  |
  ```
- **繞 Y 軸**
  ```
  |  cosθ  0  sinθ |
  |   0    1   0   |
  | -sinθ  0  cosθ |
  ```
- **繞 Z 軸**：跟 2D 旋轉一樣（左上 2×2）

（以上為 3×3 核心，塞進 4×4 的左上角。）

### 旋轉的三種表示法

1. **Euler angles**（繞 X/Y/Z 依序轉）— 直覺，但有 **Gimbal Lock**（萬向鎖）：
   某些角度下兩個旋轉軸重合，少掉一個自由度。
2. **Axis-Angle**（繞任意軸 n 轉 θ）— 沒有鎖問題。
3. **Quaternions 四元數** — 業界動畫/相機插值的標準：
   無 gimbal lock、好做平滑插值（slerp）、數值穩定。課程通常點到為止。

### 法向量的變換：要用 inverse-transpose（重要！）

頂點位置用 `M` 變換，但**法向量不能直接乘 M**。
若 M 含非均勻縮放（sx≠sy≠sz），直接乘會讓法向量不再垂直於表面。

正確做法：法向量乘 **`(M⁻¹)ᵀ`**（M 的反矩陣的轉置）。

> 直覺：法向量是「表面的垂直方向」，它跟著表面的「傾斜」走而不是跟著點走。
> 純旋轉時 `(M⁻¹)ᵀ = M`（正交矩陣），所以只有含縮放/切變時才看得出差別。
> 我們 miniGPU 的茶壺目前只有旋轉+平移，所以直接用 model 轉法向量剛好沒錯；
> 一旦加非均勻縮放就得換成 inverse-transpose。

### 座標系接力（Transformation Pipeline 的骨架）

```
物件座標 (Object/Model space)
   ↓ ×Model 矩陣（擺到場景：位置/旋轉/縮放）
世界座標 (World space)
   ↓ ×View 矩陣（lookAt：把相機當原點）
相機座標 (Camera/Eye/View space)
   ↓ ×Projection 矩陣（透視或正交）
裁剪座標 (Clip space)
```

#### 第一棒細說：物件座標 ×Model = 世界座標

每個 3D 模型（茶壺、角色…）都是在自己的**本地座標系**裡建出來的：
原點通常在模型中心或腳底，頂點座標都是相對這個原點量的。例如茶壺的
壺嘴頂點可能是 `(1.2, 0.4, 0)`——這個數字只在「茶壺自己的座標系」裡有意義。

**Model 矩陣的工作就是把這些本地座標「擺進共同的世界舞台」**：決定這顆
茶壺要放在世界的哪個位置、轉多少角度、放大縮小幾倍。

寫法（OpenGL column-major，矩陣在左、頂點在右）：
```
p_world = Model × p_object
```
（若用 D3D 的 row-major 慣例則寫 `p_object × Model`，矩陣是轉置的。概念一樣。）

Model 矩陣本身通常由三個基本變換組合而成，順序是 **先縮放、再旋轉、最後平移**：
```
Model = Translate × Rotate × Scale
```
讀法（右到左，跟 05 集的組合律一致）：頂點先在原地縮放 → 再繞原點旋轉
→ 最後整個搬到世界裡的目標位置。順序不能亂：如果先平移再旋轉，物件會
「繞著世界原點公轉」而不是「自轉」，位置就跑掉了。

**為什麼需要世界座標這一層？** 因為場景裡有很多物件，各自有自己的本地
座標系；打光的光源、相機也都放在世界裡。只有先把所有東西都換算到「同一
個世界座標系」，大家才有共同的尺標可以互相比較（誰在誰左邊、光從哪照來）。
所以世界座標是「把各說各話的本地座標，統一翻譯成同一種語言」。

> miniGPU 對照：茶壺的 `Model` 在 `teapot.cpp` 目前只用了旋轉（`rotateY`）+
> 平移，沒有縮放。頂點打光要用的法向量也是在這一步用 model 矩陣轉到世界空間
> （見上面 inverse-transpose 那段——因為沒有非均勻縮放，直接乘剛好正確）。

#### 後面兩棒
- **View 矩陣 = lookAt(eye, center, up)**：由相機的位置與朝向反推，
  本質是「把世界搬到相機的視角」，所以是相機擺放的**反變換**
  （相機往右移＝整個世界相對往左移）。輸出相機座標。
- **Projection 矩陣**：把相機座標投影到 clip space（詳見 07 集的投影原理）。
- 這條鏈就是下一集 GPU pipeline 的 vertex 階段在做的事。

### Column-major vs Row-major（踩坑點）
OpenGL/GLSL 用 **column-major**，矩陣乘向量寫成 `M * v`。
D3D 傳統 row-major，寫 `v * M`。搬程式碼時矩陣要不要轉置常在這裡出包。

---

## 07 — GPU Pipeline

一顆三角形從頂點資料到螢幕像素，會走過這條「圖形管線」。
分成 **可程式化階段（你寫 shader）** 和 **固定功能階段（硬體自動做）**。

### 完整流程（一個頂點/三角形的旅程）

```
[頂點資料] Vertex Buffer（位置、法向量、UV、顏色…）
     │
     ▼
① Vertex Shader（可程式）— 每個頂點跑一次
     │   做 MVP 變換：gl_Position = Proj × View × Model × pos
     │   輸出 clip space 座標 + 給後面用的 varying（世界座標/法向量/UV）
     ▼
② Primitive Assembly + Clipping（固定）
     │   把頂點組成三角形，裁掉視錐外的部分
     ▼
③ Perspective Divide（固定）— ÷w
     │   clip space → NDC（Normalized Device Coordinates，-1~1 方塊）
     │   ★「近大遠小」透視感在這一步引爆（w 越大＝越遠＝被除得越小）
     ▼
④ Viewport Transform（固定）
     │   NDC → 螢幕像素座標（配合視窗寬高）
     ▼
⑤ Rasterization 光柵化（固定）
     │   三角形掃成一堆 fragment（候選像素）
     │   用重心座標（barycentric）內插頂點屬性：法向量/UV/顏色
     ▼
⑥ Fragment Shader（可程式）— 每個 fragment 跑一次
     │   算這個像素的最終顏色：打光、貼圖取樣、材質
     ▼
⑦ Per-Fragment Ops（固定）
     │   Depth test（Z-buffer，擋住被遮的像素）
     │   Blending（半透明混合）、Stencil test
     ▼
[Framebuffer] → 螢幕
```

### 兩類階段的分工哲學

| | 你寫（可程式 shader） | 硬體做（固定功能） |
|---|---|---|
| 有哪些 | Vertex / Fragment（還有 Geometry/Tessellation/Compute） | Clip、÷w、Viewport、Rasterize、Depth/Blend |
| 為什麼 | 需要彈性：變換方式、打光模型任你定 | 又固定又超重複，做成硬體最快 |

**分工黃金律**（跟我們 TG 討論的一樣）：
- CPU 算「一幀只需一次」的（相機 View/Proj、每物件 Model）
- Vertex shader 做「每個頂點都要」的乘法（投影變換）
- Fragment shader 做「每個像素都要」的（打光、貼圖）
- 越往下游，執行次數越爆炸（頂點數 → 像素數），所以固定功能化

### Projection 投影矩陣：兩種投影的原理

兩種投影的差別，濃縮成一句話就是：**投影矩陣有沒有把深度 z 塞進 w 分量。**

#### 正交投影 Orthographic（平行投影）

**原理**：投影線互相**平行**，不會聚到一個眼睛點——像太陽光直射。
它做的事就是把一個「長方體視野」(由 left/right/bottom/top/near/far 六個面
框出的盒子) **線性地**搬進 -1~1 的標準方塊 (NDC)。

機制只有兩步：**平移**（把盒子中心移到原點）+ **縮放**（每軸縮到 -1~1）：
```
| 2/(r-l)    0       0     -(r+l)/(r-l) |
|   0     2/(t-b)    0     -(t+b)/(t-b) |
|   0       0    -2/(f-n)  -(f+n)/(f-n) |
|   0       0       0            1      |
```
**關鍵**：最後一列是 `0 0 0 1` → 輸出的 `w` 恆為 **1**，之後的 ÷w 等於沒除。
所以螢幕上的 x, y 位置**完全不受 z（遠近）影響** → 遠的物件和近的一樣大。
用途：CAD/工程製圖、2D 遊戲、UI、等角視 (isometric) 遊戲。

#### 透視投影 Perspective

**原理**：投影線全部**聚到眼睛**（center of projection），模擬針孔相機/人眼。
核心是國中的**相似三角形**：相機空間一個點 `(x, y, z)`，投到影像平面上是
```
x' = x × (d / z)      y' = y × (d / z)
```
`d` 是眼睛到影像平面的距離。看出來了嗎——**除以 z 就是透視的靈魂**：
z 越大（越遠）→ 除得越多 → 投影後越小 → 這就是「近大遠小」。

**但矩陣是線性運算，做不出「除法」。** 所以用一個巧妙的招數：
讓投影矩陣把 `-z` **抄進輸出的 `w` 分量**，然後交給管線後面固定功能的
**perspective divide（÷w）** 去真正執行那個除法。這正是為什麼整條管線
需要齊次座標 + ÷w 這一步——就是為了實現透視。
```
| 2n/(r-l)   0      (r+l)/(r-l)      0      |
|   0     2n/(t-b)  (t+b)/(t-b)      0      |
|   0       0      -(f+n)/(f-n)  -2fn/(f-n) |
|   0       0          -1            0      |   ← 這列的 -1 把 -z 抄進 w_clip
```
**關鍵**：最後一列是 `0 0 -1 0`，所以輸出 `w_clip = -z_eye`。視錐 (frustum，
一個被截頂的金字塔) 被映射進 -1~1 方塊，但 **z 方向是非線性的**（近平面
深度精度高、遠平面精度低——這也是 Z-fighting 常發生在遠處的原因）。
由 `fov, aspect, near, far` 決定視錐形狀。

#### 一句話對比

| | 正交 Orthographic | 透視 Perspective |
|---|---|---|
| 投影線 | 平行 | 聚到眼睛 |
| 最後一列 | `0 0 0 1` | `0 0 -1 0` |
| 輸出 w | 恆為 1 | = -z |
| ÷w 效果 | 等於沒除 | 除以深度 |
| 視覺 | 遠近一樣大 | 近大遠小 |
| 用途 | CAD/2D/UI/等角 | 3D 場景、遊戲、模擬人眼 |

**投影矩陣共同做的兩件事**：把視野體積壓成 -1~1 的標準方塊 + （透視才有）
為 ÷w 除法鋪路。差別純粹在「最後一列有沒有把 z 搬進 w」。

### Clip space 與 w 的意義
Vertex shader 輸出的 `gl_Position` 是四維 `(x, y, z, w)` 的 **clip space**。
- 裁剪在這裡做（趁還沒除 w，數學乾淨）
- 之後 `÷w` 才進 NDC
- `z/w` 存進 Z-buffer 當深度

### Z-buffer（深度緩衝）
每個像素存一個深度值。新 fragment 要畫時比深度：更近才覆蓋、更遠就丟棄。
這是「誰擋住誰」的解法，硬體自動做（步驟⑦）。

### 對應 OpenGL API（course 常帶）
- Vertex Buffer Object (VBO) / Vertex Array Object (VAO) 餵頂點
- `glDrawArrays / glDrawElements` 觸發整條管線
- Uniform / Uniform Buffer Object (UBO) 餵矩陣
- Shader 用 GLSL 寫，`gl_Position` 是 vertex shader 的內建輸出

---

## 附錄：概念 → miniGPU 程式碼對照表

我們自己那顆軟體 GPU（畫 Utah 茶壺，跟 Cem Yuksel 同源 🫖）
剛好把上面每個階段都實作了一遍，可以邊讀筆記邊對程式碼：

| 課程概念 | miniGPU 位置 |
|---|---|
| 4×4 矩陣、lookAt、perspective、rotateX/Y | `host/math3d.h` |
| Model/View/Proj 合成 MVP（CPU 端） | `gl/gl.cpp` → `mvp = proj*view*model` |
| 相機設定（觀察者） | `teapot.cpp` → `lookAt({0,2.2,7.5},…)` + `perspective(0.9,…)` |
| ① Vertex Shader（MVP 變換） | `shaders/mvp.vert` → `gl_Position = mvp*vec4(pos,1)`，跑在 `gpu/vertex_stage` |
| ③ Perspective divide (÷w) | `gpu/vertex_stage`（NDC 輸出） |
| ⑤ Rasterization + 重心內插 | `gpu/rasterizer` |
| ⑥ Fragment Shader（Lambert 打光/貼圖） | `shaders/light.frag`，跑在 `gpu/fragment_stage` |
| ⑦ Depth test（Z-buffer） | `gpu/zbuffer.h` |
| Framebuffer → PNG | `gpu/framebuffer` + `stb_image_write.h` |
| 法向量用 model 轉（目前無縮放，剛好對） | `gpu/vertex_stage`（見 06 集 inverse-transpose 註記） |

**與真硬體的唯一簡化**：miniGPU 在 CPU 先把三顆矩陣合成一顆 MVP；
真引擎多半只傳 View/Proj + 每物件 Model，讓 vertex shader 自己乘
`proj*view*model*pos`（因為打光需要中間的世界/相機空間座標）。
其餘 ①→⑦ 這條鏈，miniGPU 是照真管線走的。

---

## 一頁速記（考前/複習用）

- **變換 = 矩陣乘座標**；齊次座標讓平移也變成矩陣乘 → 全部可串接
- **順序有意義**：`A·B ≠ B·A`，右邊先作用；繞任意點轉 = `T(c)·R·T(-c)`
- **3D**：4×4 矩陣；旋轉有 Euler(有 gimbal lock)/axis-angle/quaternion
- **法向量**要用 `(M⁻¹)ᵀ`，不能直接乘 M（純旋轉時例外）
- **座標接力**：Object →(Model)→ World →(View/lookAt)→ Camera →(Proj)→ Clip
- **GPU 管線**：VS(MVP) → clip → ÷w(NDC) → viewport → 光柵化 → FS(打光) → Z-test → framebuffer
- **可程式**=VS/FS 你寫；**固定功能**=clip/÷w/rasterize/depth 硬體做
- **透視感**由 Projection 埋、由 ÷w 引爆；正交投影則無近大遠小
