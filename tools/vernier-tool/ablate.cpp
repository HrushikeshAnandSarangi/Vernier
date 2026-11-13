#include "ablate.h"
#include "Vernier/Schema.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

using namespace llvm;

namespace vernier {

static int64_t countObjectInstructions(const std::string &ObjPath) {
  auto ObjdumpPath = sys::findProgramByName("llvm-objdump");
  if (!ObjdumpPath)
    ObjdumpPath = sys::findProgramByName("objdump");
  if (!ObjdumpPath)
    return -1;

  SmallString<128> DisasmOut;
  sys::fs::createTemporaryFile("vernier_disasm", "txt", DisasmOut);

  std::optional<StringRef> Redirects[] = {
      std::nullopt, StringRef(DisasmOut), std::nullopt};
  std::vector<StringRef> Args = {*ObjdumpPath, "-d", ObjPath};

  int Ret = sys::ExecuteAndWait(*ObjdumpPath, Args, std::nullopt, Redirects);
  if (Ret != 0)
    return -1;

  auto BufferOrError = MemoryBuffer::getFile(DisasmOut);
  if (!BufferOrError)
    return -1;

  int64_t Count = 0;
  StringRef Content = (*BufferOrError)->getBuffer();
  SmallVector<StringRef, 256> Lines;
  Content.split(Lines, '\n');

  for (const auto &Line : Lines) {
    StringRef Trimmed = Line.trim();
    size_t Colon = Trimmed.find(':');
    if (Colon != StringRef::npos && Colon > 0) {
      StringRef Prefix = Trimmed.substr(0, Colon).trim();
      bool AllHex = !Prefix.empty();
      for (char C : Prefix) {
        if (!isxdigit(C)) {
          AllHex = false;
          break;
        }
      }
      if (AllHex && Trimmed.size() > Colon + 1) {
        ++Count;
      }
    }
  }

  sys::fs::remove(DisasmOut);
  return Count;
}

static uint64_t getFileSizeBytes(const std::string &Path) {
  uint64_t Size = 0;
  if (!sys::fs::file_size(Path, Size))
    return Size;
  return 0;
}

static std::string buildCommandString(const std::vector<std::string> &Tokens,
                                      const std::string &OutObj) {
  std::string Result;
  bool FoundOut = false;
  for (size_t i = 0; i < Tokens.size(); ++i) {
    if (Tokens[i] == "-o" && i + 1 < Tokens.size()) {
      Result += "-o " + OutObj + " ";
      ++i;
      FoundOut = true;
    } else {
      Result += Tokens[i] + " ";
    }
  }
  if (!FoundOut) {
    Result += "-o " + OutObj;
  }
  return Result;
}

static std::string formatSigned(int64_t V) {
  if (V >= 0)
    return "+" + std::to_string(V);
  return std::to_string(V);
}

int runAblate(const std::string &JsonPath, size_t TopN,
              const std::string &CmdOverride) {
  auto BufferOrError = MemoryBuffer::getFile(JsonPath);
  if (!BufferOrError) {
    errs() << "Vernier: Failed to read '" << JsonPath << "': "
           << BufferOrError.getError().message() << "\n";
    return 1;
  }

  auto Parsed = json::parse((*BufferOrError)->getBuffer());
  if (!Parsed) {
    errs() << "Vernier: Failed to parse JSON: "
           << toString(Parsed.takeError()) << "\n";
    return 1;
  }

  json::Object *Root = Parsed->getAsObject();
  if (!Root) {
    errs() << "Vernier: Root JSON element is not an object\n";
    return 1;
  }

  json::Object *Metadata = Root->getObject("metadata");
  if (!Metadata) {
    errs() << "Vernier: Missing 'metadata' section in JSON\n";
    return 1;
  }

  std::vector<std::string> CmdLine;
  if (json::Array *CmdArr = Metadata->getArray("command_line")) {
    for (const auto &Val : *CmdArr) {
      if (auto S = Val.getAsString())
        CmdLine.push_back(S->str());
    }
  }

  if (CmdLine.empty() && CmdOverride.empty()) {
    errs() << "Vernier: No compiler command line in JSON metadata. Provide --cmd.\n";
    return 1;
  }

  // 1. Run baseline compilation to get baseline object
  std::string BaselineObj = "/tmp/vernier_base.o";
  std::string BaseCmdStr;
  if (!CmdOverride.empty()) {
    BaseCmdStr = CmdOverride + " -o " + BaselineObj;
  } else {
    BaseCmdStr = buildCommandString(CmdLine, BaselineObj);
  }

  outs() << "Vernier: Running baseline compilation...\n";
  int BaseRet = std::system(BaseCmdStr.c_str());
  if (BaseRet != 0) {
    errs() << "Vernier: Baseline command failed with exit code " << BaseRet
           << ":\n  " << BaseCmdStr << "\n";
    return 1;
  }

  uint64_t BaselineSize = getFileSizeBytes(BaselineObj);
  int64_t BaselineInsts = countObjectInstructions(BaselineObj);

  outs() << formatv("  Baseline: size = {0} bytes, instructions = {1}\n",
                    BaselineSize, BaselineInsts);

  // 2. Rank candidate passes by significance
  std::map<std::string, double> PassScores;
  if (json::Array *Invs = Root->getArray("invocations")) {
    for (const auto &InvVal : *Invs) {
      if (const json::Object *Inv = InvVal.getAsObject()) {
        const json::Object *Change = Inv->getObject("change");
        if (Change && Change->getBoolean("changed").value_or(false)) {
          auto PassIdOpt = Inv->getString("pass_id");
          if (!PassIdOpt)
            continue;
          std::string PassID = PassIdOpt->str();

          // Skip wrapper and adaptor passes for ablation
          if (PassID.find("Adaptor") != std::string::npos ||
              PassID.find("Wrapper") != std::string::npos ||
              PassID.find("PassManager") != std::string::npos)
            continue;

          double Score = 1.0;
          if (const json::Object *Sig = Inv->getObject("significance")) {
            if (const json::Object *Struct = Sig->getObject("structural")) {
              if (auto DInst = Struct->getInteger("delta_instructions"))
                Score += std::abs(*DInst);
            }
            if (const json::Object *Cost = Sig->getObject("cost")) {
              if (auto DHot = Cost->getNumber("weighted_hotpath_cost_delta"))
                Score += std::abs(*DHot);
            }
          }
          PassScores[PassID] += Score;
        }
      }
    }
  }

  std::vector<std::pair<std::string, double>> Ranked(PassScores.begin(),
                                                     PassScores.end());
  std::sort(Ranked.begin(), Ranked.end(),
            [](const auto &A, const auto &B) { return A.second > B.second; });

  size_t CandCount = std::min(TopN, Ranked.size());
  outs() << formatv("Vernier: Selected {0} candidate passes for ablation...\n",
                    CandCount);

  json::Array AblationCandidates;

  for (size_t idx = 0; idx < CandCount; ++idx) {
    const std::string &PassID = Ranked[idx].first;
    std::string AblatedObj = formatv("/tmp/vernier_ablate_{0}.o", idx).str();
    std::string AblatedCmd;
    if (!CmdOverride.empty()) {
      AblatedCmd = CmdOverride + " -o " + AblatedObj;
    } else {
      AblatedCmd = buildCommandString(CmdLine, AblatedObj);
    }

    std::string FullExec = "VERNIER_DISABLE_PASS=" + PassID + " " + AblatedCmd;
    outs() << formatv("  Ablating '{0}'...\n", PassID);

    int Ret = std::system(FullExec.c_str());
    if (Ret == 0 && sys::fs::exists(AblatedObj)) {
      uint64_t AblatedSize = getFileSizeBytes(AblatedObj);
      int64_t AblatedInsts = countObjectInstructions(AblatedObj);
      int64_t DeltaSize = static_cast<int64_t>(AblatedSize) -
                          static_cast<int64_t>(BaselineSize);
      int64_t DeltaInsts = AblatedInsts - BaselineInsts;

      outs() << "    -> size delta: " << formatSigned(DeltaSize)
             << " B, inst delta: " << formatSigned(DeltaInsts) << "\n";

      AblationCandidates.push_back(json::Object{
          {"pass_id", PassID},
          {"ablated_size_bytes", static_cast<int64_t>(AblatedSize)},
          {"ablated_machine_instructions", AblatedInsts},
          {"delta_size_bytes", DeltaSize},
          {"delta_machine_instructions", DeltaInsts},
          {"status", "success"},
      });
      sys::fs::remove(AblatedObj);
    } else {
      outs() << "    -> failed or required pass\n";
      AblationCandidates.push_back(json::Object{
          {"pass_id", PassID},
          {"ablated_size_bytes", 0},
          {"ablated_machine_instructions", 0},
          {"delta_size_bytes", 0},
          {"delta_machine_instructions", 0},
          {"status", "failed_or_required"},
      });
    }
  }

  // 3. Update Root JSON with ablation results
  json::Object AblationObj{
      {"baseline",
       json::Object{
           {"object_size_bytes", static_cast<int64_t>(BaselineSize)},
           {"machine_instructions", BaselineInsts},
       }},
      {"candidates", std::move(AblationCandidates)},
  };

  (*Root)["ablation"] = std::move(AblationObj);

  // Write updated JSON back to JsonPath
  std::error_code EC;
  raw_fd_ostream OS(JsonPath, EC, sys::fs::OF_None);
  if (EC) {
    errs() << "Vernier: Failed to update '" << JsonPath
           << "': " << EC.message() << "\n";
    return 1;
  }

  OS << formatv("{0:2}\n", json::Value(std::move(*Root)));
  OS.flush();

  outs() << "Vernier: Ablation block written into " << JsonPath << "\n";
  sys::fs::remove(BaselineObj);
  return 0;
}

} // namespace vernier
