# 針對 LLVM 的 C++ 導讀

> 目的：**讀懂 LLVM 原始碼**所需的那一套 C++——不是教你全部 C++，是抓住 LLVM
> 「刻意跟標準 C++ 不一樣」的慣用法與設計模式。讀完這份，你打開 LLVM（或我們自己的
> ToyGPU backend）的 `.cpp`，大部分寫法都認得。
>
> 核心心法：**LLVM 是 C++ 的一個「方言」**——它關掉 RTTI、關掉例外、不用 std 容器、
> 自帶一套慣用法。學會這個方言，就讀得懂這 20 年的老程式庫。實例大量取自本 repo 的
> `compiler/backend/ToyGPU/`（我們自己寫的真 LLVM target）。

---

## Part 0 — 心態：為什麼 LLVM 的 C++ 長得不一樣

LLVM 是效能極度敏感、跑在編譯器熱路徑上的大型 C++ 庫。它做了三個「反直覺」的取捨：

1. **關掉 C++ RTTI**（`-fno-rtti`）→ 不能用 `dynamic_cast`、`typeid`。改用自己的
   `isa<>/cast<>/dyn_cast<>`（Part 1）。原因：dynamic_cast 慢又肥。
2. **關掉例外**（`-fno-exceptions`）→ 沒有 `try/catch/throw`。錯誤用 `assert` /
   `Error` / `Expected<T>` 處理（Part 5）。
3. **不用 std 容器當主力** → 用自己的 ADT（`SmallVector`、`StringRef`…），為了記憶體
   佈局與 cache 效率（Part 2）。

所以你讀 LLVM 時「找不到 dynamic_cast、找不到 try、看到一堆沒看過的容器」都是正常的——
那就是這個方言。

---

## Part 1 — 自訂 RTTI：`isa` / `cast` / `dyn_cast`（最重要，先學這個）

LLVM 到處是「開放的型別階層」：`Value` → `Instruction` → `BinaryOperator`…；
`SDNode` → `ConstantSDNode`…。要「問這個東西是不是某個子型別、並安全轉過去」，
LLVM 用三個模板函式（**不是** dynamic_cast）：

```cpp
isa<T>(x)        // 回 bool：x 是不是 T？
cast<T>(x)       // 「我確定是」→ 直接轉（debug 版會 assert，release 不檢查）
dyn_cast<T>(x)   // 「試試看」→ 是就回 T*，不是回 nullptr
```

慣用寫法（LLVM 最常見的一行）：
```cpp
if (auto *C = dyn_cast<ConstantSDNode>(V)) {
    // V 真的是常數 → 用 C
    uint64_t n = C->getZExtValue();
}
```

還有兩個變體：
```cpp
dyn_cast_or_null<T>(x)  // x 可能是 nullptr 也 OK
cast_or_null<T>(x)
```

**背後怎麼運作**：每個基底類別有一個「Kind / ID enum」+ 每個子類別提供一個
`static bool classof(const Base *)`。`isa<T>` 就是呼叫 `T::classof(x)` 去比對那個
enum。所以你自己設計型別階層要支援 isa 時，就是加一個 kind enum + classof：

```cpp
class Shape {
public:
  enum Kind { SK_Circle, SK_Square };
  Kind getKind() const { return K; }
  Shape(Kind k) : K(k) {}
private:
  const Kind K;
};
class Circle : public Shape {
public:
  Circle() : Shape(SK_Circle) {}
  static bool classof(const Shape *S) { return S->getKind() == SK_Circle; }
};
// 用：if (auto *c = dyn_cast<Circle>(shape)) ...
```

👉 **看到 `dyn_cast` / `isa` / `classof` / 一個叫 Kind 的 enum，就知道這是 LLVM 的
自訂 RTTI**。這是讀 LLVM 的第一把鑰匙。

---

## Part 2 — LLVM 自己的容器 (ADT, `llvm/ADT/`)

LLVM 幾乎不用 `std::vector` / `std::string` 當介面。認得這幾個就夠讀 90% 的 code：

| LLVM 型別 | 相當於 std | 重點 |
|---|---|---|
| `SmallVector<T,N>` | `vector<T>` | 前 N 個元素放在 stack（不配堆），超過才配堆。小集合超快 |
| `SmallVectorImpl<T>` | — | SmallVector 的「不含 N」基底；**函式參數收 `SmallVectorImpl<T>&`**（不綁死 N） |
| `ArrayRef<T>` | `span<const T>` | **非擁有**的唯讀視圖（指標+長度），傳陣列不複製 |
| `MutableArrayRef<T>` | `span<T>` | 可寫版 |
| `StringRef` | `string_view` | **非擁有**字串視圖，傳字串不複製、不保證 null 結尾 |
| `Twine` | — | 「延遲字串串接」，`A + B + C` 不會產生中間字串 |
| `DenseMap<K,V>` | `unordered_map` | 開放定址、cache 友善的雜湊表（主力 map） |
| `StringMap<V>` | `map<string,V>` | 鍵是字串專用 |
| `SmallString<N>` | `string` | 小字串放 stack |
| `SetVector` / `SmallSet` | set | 有序集合 / 小集合 |
| `iplist` / `simple_ilist` | intrusive list | 侵入式雙向鏈（見 Part 3） |
| `PointerIntPair<P,n,I>` | — | 把一個小整數塞進指標的低位（省記憶體） |
| `PointerUnion<A,B>` | variant | 「這個指標是 A 或 B」二選一 |
| `std::optional<T>` | optional | 舊版叫 `llvm::Optional`，現已對齊標準 |

兩個最容易誤會的：
- **`StringRef` / `ArrayRef` 不擁有資料**——它只是「指過去」。所以不能回傳一個指向
  local 的 StringRef（懸空）。看到參數是 `StringRef`，代表「借你看、我不複製」。
- **函式參數用 `SmallVectorImpl<T>&` 而非 `SmallVector<T,N>&`**：這樣呼叫端的 N 可以
  不同。這是 LLVM 慣例，看到就懂。

我們 ToyGPU backend 的真實例（`ToyGPUISelLowering.cpp`）：
```cpp
SDValue LowerCall(CallLoweringInfo &CLI, SmallVectorImpl<SDValue> &InVals) const;
StringRef Name = GA ? GA->getGlobal()->getName() : StringRef();
```
`SmallVectorImpl<SDValue>&` 收出參數、`StringRef` 借用名字不複製——很典型。

---

## Part 3 — 記憶體與所有權：「誰負責 delete？」

LLVM 用**大量 raw 指標**，但那不代表沒管理——raw 指標幾乎都是「借用 (non-owning)」，
擁有權在別的地方。讀 LLVM 要隨時問「這個物件誰擁有」：

| 你看到 | 意思 |
|---|---|
| `T *` （raw 指標） | **借用**，我不負責釋放。絕大多數 API 都是這個 |
| `unique_ptr<T>` | **擁有**，誰拿到誰負責（移動轉移擁有權） |
| `ilist` / 侵入式鏈 | **容器擁有節點**。例：`Instruction` 被它所在的 `BasicBlock` 的 ilist 擁有；你 `new Instruction` 後把它插進 BB，BB 就負責它的生命 |
| `BumpPtrAllocator` | **競技場配置器**：一次配一大塊、只 bump 指標分配、最後整塊丟。個別物件不 delete（例如 SelectionDAG 的 node 都從 arena 出） |
| `Context` 擁有 | 很多唯一化物件（`Type`、常數）被 `LLVMContext` 擁有，永不手動釋放 |

所以看到 `Instr->eraseFromParent()`（而不是 `delete Instr`）不用意外——那是叫「所屬
ilist 把我移除並釋放」。**LLVM 很少出現 `delete`**，因為擁有權都在容器/arena/context。

---

## Part 4 — LLVM 愛用的設計模式（你記得的那幾個）

### ① LLVM-style RTTI（開放型別階層）
就是 Part 1。當你有「一個基底、很多子類、要動態辨識」時，用 kind enum + classof +
isa/dyn_cast，取代虛擬函式 dynamic_cast。整個 IR (`Value`/`Type`/`SDNode`) 都靠它。

### ② CRTP（Curiously Recurring Template Pattern，奇異遞迴模板）
基底是模板、把「衍生類別」當模板參數傳給自己：
```cpp
template <class Derived> struct Visitor {
  void visitAll() { static_cast<Derived*>(this)->visitOne(); }  // 靜態多型
};
struct MyVisitor : Visitor<MyVisitor> { void visitOne() { /*...*/ } };
```
好處：**編譯期多型**，沒有虛擬函式的執行期成本。LLVM 的 `InstVisitor<Derived>`、
`SmallVectorImpl` 對 `SmallVector` 的關係、很多 pass 都用 CRTP。看到
`class X : public Base<X>` 這種「自己傳自己」就是 CRTP。

### ③ Visitor（訪問者）
對一個型別階層「每種節點做不同的事」。LLVM 的 `InstVisitor` 讓你
`visitLoadInst / visitStoreInst / ...`，它內部用 isa/dyn_cast 分派。SelectionDAG 的
指令選擇、type legalization 也是 visitor 味道的大 switch。我們 backend 的
`LowerOperation` 裡那個 `switch (Opcode)` 就是最樸素的 visitor。

### ④ Factory + 自我註冊（Registration）
LLVM 用「靜態全域物件在載入時把自己註冊進一個表」的手法，讓元件可插拔：
- **Target 註冊**：`RegisterTargetMachine<ToyGPUTargetMachine> X(getTheToyGPUTarget());`
  —— 一個 static 物件，建構時把這個 target 塞進 `TargetRegistry`。之後 `llc -mtriple`
  就能查表找到它。
- **Pass 註冊**：`INITIALIZE_PASS(...)` 巨集，同樣是自我註冊。

看到「檔案尾巴一個 static 全域變數、型別是 RegisterXxx」就是這個模式——它讓 LLVM 不用
改核心就能加新 target / pass。

### ⑤ RAII / PassManager pipeline
- **RAII**：資源綁生命週期（scope 結束自動收），LLVM 到處是（如各種 `Scoped*` guard）。
- **PassManager**：把一串 pass 串成 pipeline，依序跑、管理分析結果的失效——是
  Pipeline + Observer 的綜合體。你 `llc` 跑的就是一條 codegen pass pipeline。

---

## Part 5 — 錯誤處理（沒有例外）

LLVM 關了例外，錯誤分三種層級：

1. **程式邏輯錯誤（bug）→ `assert`**
   ```cpp
   assert(idx < N && "index out of range");   // 慣例：條件 && "訊息"
   ```
   release build 會被關掉，只在 debug 生效。

2. **「不該走到這」→ `llvm_unreachable`**
   ```cpp
   switch (op) {
     case A: ...; 
     default: llvm_unreachable("unknown opcode");
   }
   ```
   標記邏輯上不可能的分支；真的走到就 abort（連 release 都是）。

3. **可回報的執行期錯誤 → `Error` / `Expected<T>`**（可恢復、呼叫端「必須檢查」）
   ```cpp
   Expected<int> parse(StringRef s);       // 回傳「int 或錯誤」
   auto r = parse(s);
   if (!r) return r.takeError();           // 有錯，往上傳
   int v = *r;                             // 沒錯，取值
   ```
   `Expected<T>` = 「T 或 Error」；`Error` 是必須被消費的錯誤物件（沒檢查會 assert）。
   還有 `ErrorOr<T>`（配 std::error_code）、`cantFail(...)`（我保證不會錯）。

4. **編譯器級的致命錯誤 → `report_fatal_error`**
   我們 backend 就用它：
   ```cpp
   if (!Ins.empty())
     report_fatal_error("ToyGPU: shader main() takes no arguments");
   ```
   直接印訊息並中止——適合「這個 target 根本不支援」的情況。

👉 看到 `assert` / `llvm_unreachable` / `Expected<T>` / `report_fatal_error` 就是
LLVM 的錯誤模型，別找 try/catch。

---

## Part 6 — 慣用法與工具（讀 code 常撞到）

- **range-for + iterator_range**：`for (auto &I : *BB)` 走訪 BasicBlock 的指令；
  `for (auto &Arg : F.args())`。LLVM 大量提供 `xxx_range()` 回傳可 for 的範圍。
- **`make_range(begin, end)` / `llvm::reverse(x)` / `enumerate(x)`**：範圍工具。
- **特化 traits 做泛型演算法**：
  - `GraphTraits<GraphT>` 特化 → 讓通用圖演算法（DFS、支配樹）套用到你的圖
  - `DenseMapInfo<KeyT>` 特化 → 讓你的型別能當 DenseMap 的鍵
  - 看到 `template<> struct GraphTraits<X>` 就是「我要把 X 餵給通用演算法」
- **除錯/統計巨集**：
  ```cpp
  #define DEBUG_TYPE "toygpu"
  LLVM_DEBUG(dbgs() << "lowering " << Name << "\n");   // -debug-only=toygpu 才印
  STATISTIC(NumLowered, "Number of calls lowered");     // -stats 統計
  ```
- **命令列選項**：`static cl::opt<bool> Flag("my-flag", cl::desc("..."));` —— LLVM 工具
  的參數都這樣宣告，散在各檔、自動註冊。
- **`#include "X.inc"`**：這是 **TableGen 生成**的檔（從 `.td` 產出）。你在 backend 看到
  include 一個 `.inc`、裡面是自動生成的暫存器/指令表——那不是手寫的，別去改它，改 `.td`。

---

## Part 7 — 實戰拆解：讀我們 ToyGPU backend 的一段真 code

`compiler/backend/ToyGPU/ToyGPUISelLowering.cpp` 的 `LowerCall`（把 shader 對
`@toygpu.input/uniform/output` 的呼叫，換成 target node）：

```cpp
SDValue ToyGPUTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                        SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;                    // (a) 借用參照，不擁有
  SDLoc DL = CLI.DL;
  SDValue Chain = CLI.Chain;

  auto *GA = dyn_cast<GlobalAddressSDNode>(CLI.Callee);   // (b) 自訂 RTTI：試轉
  StringRef Name = GA ? GA->getGlobal()->getName()        // (c) StringRef 借用名字
                      : StringRef();

  auto constIndex = [&](const SDValue &V) -> SDValue {     // (d) lambda + 捕獲
    auto *C = dyn_cast<ConstantSDNode>(V);                 //     再一次 dyn_cast
    if (!C)
      report_fatal_error("ToyGPU: ... index must be constant");  // (e) 無例外，直接致命
    return DAG.getTargetConstant(C->getZExtValue(), DL, MVT::i32);
  };
  // ...依 Name 決定要建 LOAD_IN / LOAD_UNI / STORE_OUT 哪個 target node
}
```

逐點對照這份導讀：
- **(a)** `SelectionDAG &DAG = CLI.DAG` — 參照借用，DAG 的擁有權在別處（Part 3）
- **(b)(d)** `dyn_cast<GlobalAddressSDNode>` / `dyn_cast<ConstantSDNode>` — 自訂 RTTI，
  「試試看是不是這個 node 型別」（Part 1）
- **(c)** `StringRef Name` — 非擁有字串視圖（Part 2）
- **(e)** `report_fatal_error(...)` — 沒有 throw，非常數 index 直接中止（Part 5）
- 出參數 `SmallVectorImpl<SDValue> &InVals` — 收集結果的 out 參數慣例（Part 2）

看懂這 6 個點，你就看懂了一段真的 LLVM target code。整個 LLVM 都是這些積木的重複。

---

## Part 8 — 一頁速記 + 對照表

**std → LLVM ADT**
| 想用 | LLVM 改用 |
|---|---|
| `dynamic_cast<T*>` | `dyn_cast<T>` / `isa<T>` / `cast<T>` |
| `vector<T>` | `SmallVector<T,N>`（參數收 `SmallVectorImpl<T>&`） |
| `string` | `std::string` 少用；介面用 `StringRef`，暫存用 `SmallString<N>` |
| `string_view` | `StringRef` |
| `span<T>` | `ArrayRef<T>` / `MutableArrayRef<T>` |
| `unordered_map` | `DenseMap` / `StringMap` |
| `A + B + C`（字串） | `Twine`（延遲串接） |
| `try/catch/throw` | `assert` / `llvm_unreachable` / `Error` / `Expected<T>` |
| `dynamic polymorphism` | 常換成 CRTP（靜態多型） |

**讀 LLVM 的五把鑰匙**
1. `dyn_cast/isa/classof/Kind enum` = 自訂 RTTI（型別辨識）
2. `SmallVector/ArrayRef/StringRef` = 非擁有 or 小最佳化的容器（借用要小心懸空）
3. raw 指標=借用、`unique_ptr`=擁有、ilist/arena/context=容器擁有 →「誰 delete」
4. 設計模式：RTTI / CRTP / Visitor / 自我註冊(Factory) / PassManager
5. 沒有例外：`assert` `llvm_unreachable` `Expected<T>` `report_fatal_error`

**看到就懂**
- `class X : public Base<X>` → CRTP
- 檔尾 `static RegisterXxx<...>` → 自我註冊（可插拔 target/pass）
- `#include "*.inc"` → TableGen 生成，去改 `.td` 不要改它
- `LLVM_DEBUG(...)` / `STATISTIC(...)` / `cl::opt<>` → 除錯/統計/命令列

讀完這 8 part，打開 `compiler/backend/ToyGPU/*.cpp` 或任何 LLVM 檔，你會發現「喔，這我
認得」的比例大幅上升。剩下的就是查 API 細節，而查得動、看得懂結構，才是真正的門檻。
