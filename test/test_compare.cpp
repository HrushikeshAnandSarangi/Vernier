#include "tools/vernier-tool/compare.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <fstream>
#include <iostream>

using namespace llvm;

int main() {
  SmallString<128> JsonAPath;
  SmallString<128> JsonBPath;
  SmallString<128> OutReportPath;
  sys::fs::createTemporaryFile("test_run_a", "json", JsonAPath);
  sys::fs::createTemporaryFile("test_run_b", "json", JsonBPath);
  sys::fs::createTemporaryFile("test_report", "txt", OutReportPath);

  // Write Run A JSON
  {
    std::ofstream OutA(JsonAPath.c_str());
    OutA << R"({
      "schema": "vernier/v1",
      "metadata": { "input_file": "test.c", "target_triple": "x86_64" },
      "invocations": [
        {
          "id": 1,
          "pass_id": "SimplifyCFGPass",
          "unit": { "kind": "function", "name": "foo" },
          "timing": { "self_ns": 100, "total_ns": 100 },
          "change": { "changed": true, "after_hash": "hash_a1" }
        },
        {
          "id": 2,
          "pass_id": "InstCombinePass",
          "unit": { "kind": "function", "name": "foo" },
          "timing": { "self_ns": 200, "total_ns": 200 },
          "change": { "changed": true, "after_hash": "hash_div_A" }
        }
      ]
    })";
  }

  // Write Run B JSON (diverges at pass 2)
  {
    std::ofstream OutB(JsonBPath.c_str());
    OutB << R"({
      "schema": "vernier/v1",
      "metadata": { "input_file": "test.c", "target_triple": "x86_64" },
      "invocations": [
        {
          "id": 1,
          "pass_id": "SimplifyCFGPass",
          "unit": { "kind": "function", "name": "foo" },
          "timing": { "self_ns": 120, "total_ns": 120 },
          "change": { "changed": true, "after_hash": "hash_a1" }
        },
        {
          "id": 2,
          "pass_id": "InstCombinePass",
          "unit": { "kind": "function", "name": "foo" },
          "timing": { "self_ns": 250, "total_ns": 250 },
          "change": { "changed": true, "after_hash": "hash_div_B" }
        }
      ]
    })";
  }

  int Ret = vernier::runCompare(JsonAPath.str().str(), JsonBPath.str().str(),
                                OutReportPath.str().str());
  assert(Ret == 0 && "runCompare must return 0");

  // Read report
  std::ifstream Rep(OutReportPath.c_str());
  std::string RepContent((std::istreambuf_iterator<char>(Rep)),
                         std::istreambuf_iterator<char>());

  assert(RepContent.find("FIRST DIVERGENCE POINT") != std::string::npos &&
         "Report must identify first divergence point");
  assert(RepContent.find("InstCombinePass") != std::string::npos &&
         "Divergence must be attributed to InstCombinePass");
  assert(RepContent.find("hash_div_A") != std::string::npos);
  assert(RepContent.find("hash_div_B") != std::string::npos);

  sys::fs::remove(JsonAPath);
  sys::fs::remove(JsonBPath);
  sys::fs::remove(OutReportPath);

  std::cout << "test_compare passed all assertions!\n";
  return 0;
}
