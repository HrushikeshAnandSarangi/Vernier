#include "tools/vernier-tool/report.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <fstream>
#include <iostream>

using namespace llvm;

int main() {
  SmallString<128> JsonPath;
  SmallString<128> OutReportPath;
  sys::fs::createTemporaryFile("test_report_run", "json", JsonPath);
  sys::fs::createTemporaryFile("test_report_out", "txt", OutReportPath);

  {
    std::ofstream Out(JsonPath.c_str());
    Out << R"({
      "schema": "vernier/v1",
      "metadata": {
        "input_file": "test_report.c",
        "target_triple": "x86_64-linux",
        "total_wall_time_ns": 5000000
      },
      "invocations": [
        {
          "id": 1,
          "pass_id": "SimplifyCFGPass",
          "depth": 0,
          "unit": { "kind": "function", "name": "test_fn" },
          "timing": { "self_ns": 2000000, "total_ns": 2000000 },
          "change": { "changed": true, "before_hash": "h1", "after_hash": "h2" },
          "significance": {
            "structural": { "delta_instructions": -5, "delta_basic_blocks": -1 },
            "cost": { "tti_cost_delta": -10 }
          }
        }
      ],
      "remarks": [
        {
          "id": 1,
          "pass_name": "loop-vectorize",
          "function": "test_fn",
          "message": "vectorized loop",
          "file": "test_report.c",
          "line": 10,
          "column": 5
        }
      ]
    })";
  }

  int Ret = vernier::runReport(JsonPath.str().str(), OutReportPath.str().str());
  assert(Ret == 0 && "runReport must return 0");

  std::ifstream Rep(OutReportPath.c_str());
  std::string RepContent((std::istreambuf_iterator<char>(Rep)),
                         std::istreambuf_iterator<char>());

  assert(RepContent.find("VERNIER PIPELINE PROFILE REPORT") != std::string::npos);
  assert(RepContent.find("1. TOP HOTSPOTS") != std::string::npos);
  assert(RepContent.find("2. OPTIMIZATION SIGNIFICANCE") != std::string::npos);
  assert(RepContent.find("3. REMARKS & MISSED OPTIMIZATIONS") != std::string::npos);
  assert(RepContent.find("SimplifyCFGPass") != std::string::npos);
  assert(RepContent.find("vectorized loop") != std::string::npos);

  sys::fs::remove(JsonPath);
  sys::fs::remove(OutReportPath);

  std::cout << "test_report passed all assertions!\n";
  return 0;
}
