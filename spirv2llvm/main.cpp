//===----------------------------------------------------------------------===//
// spirv2llvm (miniGPU) — SPIR-V fragment shader → ToyGPU-ABI LLVM IR。
//
// 從 ToyGPU v1 的 pass-through walker 擴充，多支援「真的運算」：
//   OpAccessChain（取向量的某個 channel）、OpFMul/OpFAdd/OpFSub、
//   function-local 變數（OpVariable Function + OpStore/OpLoad）。
//   → 能翻譯有打光計算的 fragment shader，不只是 RGB 直通。
//
// ABI（跟 ToyGPU backend 對接）：
//   Input  channel  → @toygpu.input(i32 channel)
//   Output channel  → @toygpu.output(i32 channel, float)
//
// two-pass：pass1 建 id 表；pass2 走 body scalarize + emit .ll 文字。
//===----------------------------------------------------------------------===//
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

enum Op : uint16_t {
  OpName = 5, OpDecorate = 71, OpTypeVoid = 19, OpTypeFloat = 22,
  OpTypeInt = 21, OpTypeVector = 23, OpTypePointer = 32, OpTypeFunction = 33,
  OpVariable = 59, OpConstant = 43, OpLoad = 61, OpStore = 62,
  OpAccessChain = 65, OpCompositeExtract = 81, OpCompositeConstruct = 80,
  OpFMul = 133, OpFAdd = 129, OpFSub = 131, OpFNegate = 127,
  OpFunction = 54, OpLabel = 248, OpReturn = 253, OpFunctionEnd = 56,
};
enum StorageClass { SC_Input = 1, SC_Uniform = 2, SC_Output = 3, SC_Function = 7 };
enum { Dec_Location = 30 };

struct Word { uint16_t op, wc; const uint32_t *ops; };

// 把 float 轉成 LLVM .ll 的 hex-float 字面（= 該值的 double 位元）。
static std::string llvmFloat(float f) {
  double d = f;
  uint64_t bits;
  memcpy(&bits, &d, 8);
  char buf[32];
  snprintf(buf, sizeof buf, "0x%016llX", (unsigned long long)bits);
  return buf;
}

int main(int argc, char **argv) {
  if (argc != 3) { fprintf(stderr, "usage: spirv2llvm in.spv out.ll\n"); return 2; }
  FILE *f = fopen(argv[1], "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
  std::vector<uint32_t> w; uint32_t x;
  while (fread(&x, 4, 1, f) == 1) w.push_back(x);
  fclose(f);
  if (w.size() < 5 || w[0] != 0x07230203) { fprintf(stderr, "bad magic\n"); return 1; }

  std::vector<Word> insts;
  for (size_t i = 5; i < w.size();) {
    uint16_t wc = w[i] >> 16, op = w[i] & 0xFFFF;
    if (!wc) break;
    insts.push_back({op, wc, &w[i + 1]}); i += wc;
  }

  // ── pass 1：id 資訊 ──
  std::map<uint32_t, uint32_t> vecComp, ptrStorage, ptrPointee, varStorage;
  std::map<uint32_t, float> constF;
  std::map<uint32_t, uint32_t> constI;
  for (auto &in : insts) {
    switch (in.op) {
    case OpTypeVector: vecComp[in.ops[0]] = in.ops[2]; break;
    case OpTypePointer: ptrStorage[in.ops[0]] = in.ops[1]; ptrPointee[in.ops[0]] = in.ops[2]; break;
    case OpVariable: varStorage[in.ops[1]] = in.ops[2]; break;   // resultId → SC
    case OpConstant: {
      float v; uint32_t b = in.ops[2]; memcpy(&v, &b, 4);
      constF[in.ops[1]] = v; constI[in.ops[1]] = in.ops[2];
      break;
    }
    default: break;
    }
  }

  // ── pass 2：走 body，emit ──
  // scalarVal：id → LLVM 表示（"%tN" 或 hex-float 字面）
  std::map<uint32_t, std::string> scalarVal;
  std::map<uint32_t, std::vector<std::string>> vecChannels;       // vector id → channels
  std::map<uint32_t, std::pair<uint32_t,int>> accessChain;        // id → (baseVar, index)
  std::map<uint32_t, std::string> localVal;                        // function-local var → 目前值
  std::string body;
  int tmp = 0;
  auto fresh = [&] { return "%t" + std::to_string(tmp++); };
  // 解析一個 id 成 LLVM float 運算元字串
  auto resolve = [&](uint32_t id) -> std::string {
    if (scalarVal.count(id)) return scalarVal[id];
    if (constF.count(id)) return llvmFloat(constF[id]);
    return "0.0";
  };

  for (auto &in : insts) {
    switch (in.op) {
    case OpAccessChain: {          // resultType, id, base, index(常數)
      uint32_t id = in.ops[1], base = in.ops[2], idxId = in.ops[3];
      int idx = constI.count(idxId) ? (int)constI[idxId] : 0;
      accessChain[id] = {base, idx};
      break;
    }
    case OpLoad: {                 // resultType, id, pointer
      uint32_t id = in.ops[1], ptr = in.ops[2];
      if (accessChain.count(ptr)) {                 // 讀某個 channel
        auto [base, idx] = accessChain[ptr];
        if (varStorage.count(base) && varStorage[base] == SC_Input) {
          std::string t = fresh();
          body += "  " + t + " = call float @toygpu.input(i32 " +
                  std::to_string(idx) + ")\n";
          scalarVal[id] = t;
        }
      } else if (varStorage.count(ptr) && varStorage[ptr] == SC_Input) {
        // 讀整個 input 向量 → scalarize（pass-through shader 走這條）
        uint32_t comps = vecComp.count(ptrPointee[/*ptrType*/0]) ? 0 : 0;
        (void)comps;
        // 用向量型別推元素數：找 pointer 的 pointee 是 vector
        int n = 3;                                  // 我們的 input 都是 vec3
        std::vector<std::string> ch;
        for (int c = 0; c < n; c++) {
          std::string t = fresh();
          body += "  " + t + " = call float @toygpu.input(i32 " +
                  std::to_string(c) + ")\n";
          ch.push_back(t);
        }
        vecChannels[id] = ch;
      } else if (localVal.count(ptr)) {             // 讀 function-local 變數
        scalarVal[id] = localVal[ptr];
      }
      break;
    }
    case OpStore: {                // pointer, object
      uint32_t ptr = in.ops[0], obj = in.ops[1];
      if (varStorage.count(ptr) && varStorage[ptr] == SC_Output && vecChannels.count(obj)) {
        auto &ch = vecChannels[obj];
        for (size_t c = 0; c < ch.size(); c++)
          body += "  call void @toygpu.output(i32 " + std::to_string(c) +
                  ", float " + ch[c] + ")\n";
      } else {                     // 寫 function-local 變數：記住它的值
        localVal[ptr] = resolve(obj);
      }
      break;
    }
    case OpFMul: case OpFAdd: case OpFSub: {        // type, id, a, b
      const char *opn = in.op == OpFMul ? "fmul" : in.op == OpFAdd ? "fadd" : "fsub";
      std::string a = resolve(in.ops[2]), b = resolve(in.ops[3]);
      std::string t = fresh();
      body += "  " + t + " = " + opn + " float " + a + ", " + b + "\n";
      scalarVal[in.ops[1]] = t;
      break;
    }
    case OpCompositeExtract: {     // type, id, composite, index
      uint32_t id = in.ops[1], comp = in.ops[2], idx = in.ops[3];
      if (vecChannels.count(comp)) scalarVal[id] = vecChannels[comp][idx];
      break;
    }
    case OpCompositeConstruct: {   // type, id, comp0..N
      std::vector<std::string> ch;
      for (uint16_t k = 2; k < in.wc - 1; k++) ch.push_back(resolve(in.ops[k]));
      vecChannels[in.ops[1]] = ch;
      break;
    }
    default: break;
    }
  }

  FILE *o = fopen(argv[2], "w");
  if (!o) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
  fprintf(o,
    "; generated by spirv2llvm from %s\n"
    "declare float @toygpu.input(i32)\n"
    "declare float @toygpu.uniform(i32)\n"
    "declare void @toygpu.output(i32, float)\n\n"
    "define void @main() {\nentry:\n%s  ret void\n}\n",
    argv[1], body.c_str());
  fclose(o);
  printf("spirv2llvm: %zu instructions -> %s\n", insts.size(), argv[2]);
  return 0;
}
