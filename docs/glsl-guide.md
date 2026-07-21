# GLSL 教學 + 速查

> GLSL = **OpenGL Shading Language**，Khronos 維護的著色器語言，語法像 C，
> 跑在 GPU 上，每個頂點/每個像素各跑一遍。這份文件教「規格 (spec) 的重點」
> 和「怎麼寫」，實例直接用我們 miniGPU/minirt 的 `shaders/mvp.vert`、
> `shaders/light.frag`——學完馬上對得上我們的 code。
>
> 對應官方 spec：Khronos《The OpenGL Shading Language》。本檔以桌面 GLSL
> `#version 450`（OpenGL 4.5）為主，順帶標出 GLSL ES（手機/WebGL）差異。

---

## 0. 心智模型：shader 是什麼

一支 shader 是一個 **`main()` 函式**，GPU 對「一大堆資料點」平行地各跑一次：
- **vertex shader**：每個頂點跑一次，任務是把頂點變換到 clip space（輸出 `gl_Position`）。
- **fragment shader**：每個像素（fragment）跑一次，任務是算那個像素的顏色。

shader 之間、以及 shader 跟外界（CPU/驅動）之間，只能透過**特定的介面變數**溝通
（`in` / `out` / `uniform`）——不能亂讀亂寫記憶體，沒有指標、沒有遞迴、沒有動態配置。
這個「受限但高度平行」就是 GPU 程式的本質。

我們 miniGPU 的最小實例：

```glsl
// mvp.vert — 每個頂點跑一次
#version 450
layout(location = 0) in vec3 pos;                 // 輸入：頂點位置
layout(binding = 0) uniform UBO { mat4 mvp; };    // uniform：MVP 矩陣（每幀一次）
void main() {
    gl_Position = mvp * vec4(pos, 1.0);           // 輸出：clip space 座標
}
```

```glsl
// light.frag — 每個像素跑一次
#version 450
layout(location = 0) in vec3 vNormal;             // 從 vertex/rasterizer 內插進來的法向量
layout(location = 0) out vec4 fragColor;          // 輸出：像素顏色
void main() {
    float b = vNormal.x*0.30 + vNormal.y*0.60 + vNormal.z*0.30 + 0.45;
    fragColor = vec4(b*0.85, b*0.82, b*0.78, 1.0);
}
```

---

## 1. `#version` 與 spec 版本

第一行幾乎一定是版本宣告，決定可用語法：

```glsl
#version 450          // 桌面 OpenGL 4.5
#version 330          // 桌面 OpenGL 3.3（很常見的相容基準）
#version 300 es       // GLSL ES 3.0（WebGL2 / 手機）
#version 100          // GLSL ES 1.0（WebGL1，舊）
```

版本對應（桌面）：GL 2.0→GLSL 110、2.1→120、3.0→130、3.3→**330**、
4.0→400 … 4.5→**450**、4.6→460。3.3 之後版號跟 GL 版號同步（4.5 → 450）。

**桌面 GLSL vs GLSL ES 主要差異**：
- ES 要寫**精度限定詞**（見 §5），桌面不用。
- ES 拿掉一些桌面功能（如某些內建、double）。
- 我們的 shader 用 `#version 450`，因為要走 SPIR-V（`glslangValidator -V`）。

---

## 2. Shader 階段 (stages)

一條光柵化管線可程式的階段（依順序）：

| 階段 | 關鍵字/副檔名 | 任務 | 必需？ |
|---|---|---|---|
| Vertex | `.vert` | 頂點變換，輸出 `gl_Position` | 是 |
| Tessellation Control | `.tesc` | 決定細分程度 | 否 |
| Tessellation Evaluation | `.tese` | 算細分後的新頂點 | 否 |
| Geometry | `.geom` | 增刪幾何（一個圖元→多個） | 否 |
| Fragment | `.frag` | 每像素著色，輸出顏色 | 幾乎必需 |
| Compute | `.comp` | 通用平行運算（不在光柵管線內） | 獨立 |

最小可用組合 = **vertex + fragment**（我們 miniGPU 就是這兩支）。

---

## 3. 型別 (types)

**純量**：`bool` `int` `uint` `float` `double`

**向量**（最常用）：
- 浮點：`vec2` `vec3` `vec4`
- 整數：`ivec2/3/4`、無號 `uvec2/3/4`、布林 `bvec2/3/4`、倍精度 `dvec2/3/4`

**矩陣**：`mat2` `mat3` `mat4`（方陣），`mat2x3`…`mat4x3`（MxN，column-major）

**取樣器（貼圖）**：`sampler2D` `sampler3D` `samplerCube` `sampler2DArray`
`sampler2DShadow` …（Vulkan 下再分 `texture2D` + `sampler`）

**結構 / 陣列**：
```glsl
struct Light { vec3 pos; vec3 color; };
float weights[4];              // 固定大小陣列
vec3 offsets[] = vec3[](...);  // 由初始化推長度
```

**限制**：沒有指標、沒有遞迴、陣列一般要編譯期固定大小（例外：SSBO 的
runtime-sized array）。隱式轉換有限：`int → float` 可以，反向不行，要顯式 `float(i)`。

---

## 4. 向量、swizzle、建構

**建構子**可任意拼裝：
```glsl
vec3 a = vec3(1.0);            // (1,1,1)
vec4 b = vec4(a, 1.0);        // (a.x,a.y,a.z,1)  ← 我們 mvp.vert 就這樣把 vec3→vec4
vec2 c = vec4(b).xy;          // 取前兩個
```

**swizzle**（GLSL 最好用的語法糖）：用 `.xyzw` / `.rgba` / `.stpq` 取分量，
可**重排、重複、當左值**：
```glsl
vec4 v;
v.xyz          // 前三個 (等同 .rgb)
v.bgra         // 重排：blue,green,red,alpha
v.xxxx         // 重複：(x,x,x,x)
v.xy = vec2(0);// 當左值寫入
```
三組名字（xyzw 位置 / rgba 顏色 / stpq 貼圖座標）純粹語意方便，可混但別在同一次混用。

---

## 5. 限定詞 (qualifiers)——寫 shader 的核心

限定詞放在型別前面，決定變數「從哪來、到哪去、怎麼存」。

### 5.1 儲存限定詞 (storage)
| 限定詞 | 意思 |
|---|---|
| `in` | 輸入：vertex 讀頂點屬性；fragment 讀上一階段內插來的值 |
| `out` | 輸出：傳給下一階段，或 fragment 的最終顏色 |
| `uniform` | 一次 draw 內固定不變的常數（矩陣、光源、貼圖），CPU 設定 |
| `buffer` | SSBO，可讀寫的大塊資料（進階） |
| `const` | 編譯期常數 |
| `shared` | compute shader 內同一工作群組共享 |

`in`/`out` 就是「階段之間的接線」：vertex 的 `out vec3 vNormal` 會被內插後，
變成 fragment 的 `in vec3 vNormal`（名字/型別要對上）——這種跨階段變數舊稱 **varying**。

### 5.2 layout 限定詞
把變數綁到明確的「位置/綁點」，這是現代 GLSL（尤其走 Vulkan/SPIR-V）的關鍵：
```glsl
layout(location = 0) in vec3 pos;      // 頂點屬性 slot 0
layout(location = 0) out vec4 color;   // fragment 輸出 slot 0（= 第一張 render target）
layout(binding = 0) uniform UBO {...}; // uniform buffer 綁點 0
layout(set = 0, binding = 1) ...       // Vulkan：descriptor set
layout(std140) uniform ...             // UBO 記憶體排版規則
layout(std430) buffer ...              // SSBO 排版
layout(local_size_x = 64) in;          // compute 工作群組大小
```
我們 `mvp.vert` 的 `layout(location=0) in vec3 pos` 就是告訴管線「頂點的第 0 個屬性餵進 pos」。

### 5.3 內插限定詞（fragment 的 in）
- `smooth`（預設）：透視正確內插
- `flat`：不內插，取單一頂點的值（畫 ID、整數用）
- `noperspective`：線性但非透視正確

### 5.4 精度限定詞（GLSL ES 必寫，桌面忽略）
`highp` / `mediump` / `lowp`。ES 的 fragment 通常要先設預設精度：
```glsl
precision highp float;
```

---

## 6. 內建變數 (built-in variables)

**Vertex shader**：
- `out vec4 gl_Position`（**必寫**）：clip space 座標
- `in int gl_VertexID` / `gl_InstanceID`：頂點/實例索引
- `out float gl_PointSize`：畫點時的大小

**Fragment shader**：
- `in vec4 gl_FragCoord`：這個像素的視窗座標（.xy 像素、.z 深度、.w = 1/w）
- `in bool gl_FrontFacing`：正面/背面
- `out float gl_FragDepth`：可覆寫深度（少用）
- `in vec2 gl_PointCoord`：點圖元內座標

---

## 7. 內建函式 (built-in functions) 常用清單

**三角**：`sin cos tan asin acos atan(y,x) radians degrees`
**指數**：`pow(x,y) exp log exp2 log2 sqrt inversesqrt`
**通用**：`abs sign floor ceil round fract mod(x,y) min max clamp(x,lo,hi)`
`mix(a,b,t)`（線性插值）`step(edge,x) smoothstep(e0,e1,x)`
**幾何**（超常用於打光）：
- `length(v) distance(a,b) dot(a,b) cross(a,b) normalize(v)`
- `reflect(I,N)` 反射向量、`refract(I,N,eta)` 折射、`faceforward(N,I,Nref)`
**矩陣**：`matrixCompMult transpose(m) inverse(m) determinant(m)`
**貼圖**：`texture(sampler, uv)` `textureLod(...)` `texelFetch(...)` `textureSize(...)`

大部分函式「逐分量作用」於向量：`sqrt(vec3(...))` = 對三個分量各開根號。

> 我們 `light.frag` 手寫了 `n.x*0.3+n.y*0.6+n.z*0.3`，其實就是 `dot(n, vec3(0.3,0.6,0.3))`。
> 用內建 `dot` 會更道地——這是很好的「怎麼寫得更 GLSL」的例子（見 §11）。

---

## 8. 控制流程與函式

跟 C 幾乎一樣：`if/else`、`for`、`while`、`break`、`continue`、`return`、
三元 `?:`、`discard`（fragment 專用：丟棄這個像素，不寫入）。

自訂函式，參數可加方向限定詞 `in`（預設）/ `out` / `inout`：
```glsl
float lambert(vec3 n, vec3 L) { return max(dot(n, L), 0.0); }

void addTwo(in float a, out float result) { result = a + 2.0; }
```
**沒有遞迴**。迴圈次數最好編譯期可知（早期硬體要求，現代放寬但仍影響效能）。

---

## 9. uniform 與 uniform block (UBO)

**單一 uniform**：
```glsl
uniform mat4 uMVP;
uniform vec3 uLightDir;
uniform sampler2D uTex;
```

**uniform block（把多個 uniform 打包，一次綁一大塊）**——現代做法：
```glsl
layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
};
```
我們 `mvp.vert` 的 `layout(binding=0) uniform UBO { mat4 mvp; };` 就是一個 UBO。
`std140` 是記憶體排版規則（有 padding 對齊規則，vec3 常被當 16 bytes 對齊，
CPU 端填資料時要跟著對齊，否則會錯位——這是實務上超常見的坑）。

---

## 10. 完整範例走讀（我們的兩支 shader）

### Vertex：`mvp.vert`
```glsl
#version 450
layout(location = 0) in vec3 pos;              // ① 頂點屬性 slot0 → pos
layout(binding = 0) uniform UBO { mat4 mvp; }; // ② UBO slot0 裡有 MVP 矩陣
void main() {
    gl_Position = mvp * vec4(pos, 1.0);        // ③ MVP × (pos,1) → clip space
}
```
- ① CPU 把頂點位置餵到 location 0。
- ② 驅動每幀把 `proj*view*model` 上傳到這個 UBO。
- ③ `vec4(pos,1.0)` 把點升到齊次座標，乘 MVP 得 clip space，寫進必寫的 `gl_Position`。

### Fragment：`light.frag`
```glsl
#version 450
layout(location = 0) in vec3 vNormal;   // 從頂點內插來的法向量
layout(location = 0) out vec4 fragColor;
void main() {
    float b = vNormal.x*0.30 + vNormal.y*0.60 + vNormal.z*0.30 + 0.45; // Lambert N·L + 環境光
    fragColor = vec4(b*0.85, b*0.82, b*0.78, 1.0);                     // 上暖灰材質色
}
```
- `vNormal` 是 rasterizer 用重心座標從三頂點內插出來的。
- `b` = 法向量對固定光向的點積 + 環境光（面越朝光越亮）。
- 輸出 `vec4(rgb, 1.0)`，alpha=1 不透明。

---

## 11. 「怎麼寫得更道地」——常見寫法升級

| 手寫 | 道地 GLSL |
|---|---|
| `n.x*L.x+n.y*L.y+n.z*L.z` | `dot(n, L)` |
| 自己 clamp `max(min(x,1),0)` | `clamp(x, 0.0, 1.0)` |
| 手動線性插值 `a*(1-t)+b*t` | `mix(a, b, t)` |
| `if (x<edge) 0 else 1` | `step(edge, x)` |
| 正規化前先算長度再除 | `normalize(v)` |
| 反射手算 | `reflect(-L, N)` |

例如把我們的 `light.frag` 寫成道地版：
```glsl
const vec3 L = normalize(vec3(0.30, 0.60, 0.30));
const vec3 mat = vec3(0.85, 0.82, 0.78);
void main() {
    float b = max(dot(normalize(vNormal), L), 0.0) + 0.45;
    fragColor = vec4(b * mat, 1.0);
}
```

---

## 12. 常見坑 (gotchas)

- **忘了寫 `gl_Position`** → vertex shader 沒輸出，畫不出東西。
- **`in`/`out` 名字或型別對不上** → 跨階段接線失敗（link error 或拿到垃圾值）。
- **整數/浮點混用**：`1/2 == 0`（整數除法！），要 `1.0/2.0`。
- **std140 對齊**：`vec3` 在 UBO 常佔 16 bytes；CPU 端結構沒對齊 → 資料錯位。
- **精度（ES）**：忘了 `precision highp float;` → 手機上編不過或精度爆掉。
- **法向量沒正規化**：內插後長度會變，打光要先 `normalize`。
- **分支發散**：同一批像素走不同 if 分支會拖慢（GPU 是 SIMD）。
- **沒有遞迴 / 動態陣列**：想遞迴要改成迴圈或多 pass。

---

## 13. GLSL 在我們專案怎麼被編譯

miniGPU/minirt 不是丟給真 GPU 驅動，而是走自己的鏈（教學用）：

```
.vert/.frag (GLSL)
   │ glslangValidator -V      (Khronos 官方，GLSL→SPIR-V)
SPIR-V (.spv)                  ← Khronos 的中間表示，Vulkan 也用這個
   │ spirv2llvm               (我們寫的)
LLVM IR (.ll)
   │ llc -mtriple=toygpu      (fork 的 LLVM + 自寫 target)
ToyGPU 組語 (.s)
   │ toyasm.py
ISA binary (.bin)             ← 在 shader_core 上跑
```

所以這份 spec 學的 GLSL，寫出來的 shader 真的會經過 SPIR-V + LLVM 編成我們那顆
ToyGPU 的機器碼。想看你寫的 GLSL 變成什麼：
```bash
glslangValidator -V -S frag shaders/light.frag -o /tmp/x.spv
spirv-dis /tmp/x.spv            # 看 SPIR-V
build/spirv2llvm /tmp/x.spv /tmp/x.ll && cat /tmp/x.ll   # 看 LLVM IR
```

---

## 一頁速記

- 第一行 `#version 450`；一支 shader = 一個 `main()`，GPU 平行跑很多次
- 介面：`in`（輸入/上一階段）、`out`（輸出/下一階段）、`uniform`（每 draw 常數）
- `layout(location=/binding=)` 綁 slot；跨階段 `out→in` 名字型別要對上
- 型別：`float vec2/3/4 mat3/4 sampler2D`；swizzle `.xyzw/.rgba` 可重排重複當左值
- 必寫：vertex 的 `gl_Position`；fragment 輸出一個 `vec4` 顏色
- 打光愛用內建：`dot cross normalize reflect clamp mix step smoothstep`
- 坑：整數除法、std140 對齊、法向量要 normalize、ES 要寫 precision、沒遞迴
- 我們的鏈：GLSL → SPIR-V(glslang) → LLVM IR → ToyGPU ISA
