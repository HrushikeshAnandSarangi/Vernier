#include "report.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace llvm;

namespace vernier {

struct InvDetail {
  uint64_t Id = 0;
  std::string PassID;
  uint32_t Depth = 0;
  std::string UnitKind;
  std::string UnitName;
  uint64_t SelfNs = 0;
  uint64_t TotalNs = 0;
  bool Changed = false;
  bool UnderReporting = false;
  std::string BeforeHash;
  std::string AfterHash;
  std::string DiffId;
  std::string Diff;

  int64_t DeltaInsts = 0;
  int64_t DeltaBBs = 0;
  int64_t DeltaLoads = 0;
  int64_t DeltaStores = 0;
  int64_t DeltaAllocas = 0;
  int64_t DeltaCalls = 0;
  int64_t InlinedCalls = 0;
  int64_t VectorWidth = 1;
  int64_t TTICostDelta = 0;
  double WeightedCostDelta = 0.0;
};

struct RemarkDetail {
  uint64_t Id = 0;
  std::string PassName;
  std::string Function;
  std::string Message;
  std::string File;
  int64_t Line = 0;
  int64_t Column = 0;
};

struct AblateCandidateDetail {
  std::string PassID;
  int64_t AblatedSize = 0;
  int64_t AblatedInsts = 0;
  int64_t DeltaSize = 0;
  int64_t DeltaInsts = 0;
  std::string Status;
};

static std::string formatDuration(uint64_t Ns) {
  std::ostringstream SS;
  SS << std::fixed << std::setprecision(2);
  if (Ns < 1000) {
    SS << Ns << " ns";
  } else if (Ns < 1000000) {
    SS << (Ns / 1000.0) << " us";
  } else if (Ns < 1000000000) {
    SS << (Ns / 1000000.0) << " ms";
  } else {
    SS << (Ns / 1000000000.0) << " s";
  }
  return SS.str();
}

static std::string formatSigned(int64_t V) {
  if (V > 0)
    return "+" + std::to_string(V);
  return std::to_string(V);
}

static std::string makeProgressBar(double Fraction, int Width = 15) {
  if (Fraction < 0.0)
    Fraction = 0.0;
  if (Fraction > 1.0)
    Fraction = 1.0;
  int Filled = static_cast<int>(std::round(Fraction * Width));
  int Empty = Width - Filled;
  std::string Bar = "[";
  for (int i = 0; i < Filled; ++i)
    Bar += "#";
  for (int i = 0; i < Empty; ++i)
    Bar += ".";
  Bar += "]";
  return Bar;
}

int runReport(const std::string &JsonPath, const std::string &OutPath) {
  auto BufferOrError = MemoryBuffer::getFile(JsonPath);
  if (!BufferOrError) {
    errs() << "Vernier: Failed to read '" << JsonPath << "': "
           << BufferOrError.getError().message() << "\n";
    return 1;
  }

  auto Parsed = json::parse((*BufferOrError)->getBuffer());
  if (!Parsed) {
    errs() << "Vernier: Failed to parse JSON from '" << JsonPath << "'\n";
    return 1;
  }

  const json::Object *Root = Parsed->getAsObject();
  if (!Root) {
    errs() << "Vernier: Invalid root JSON\n";
    return 1;
  }

  // Parse Metadata
  std::string InputFile = "unknown";
  std::string TargetTriple = "unknown";
  uint64_t TotalWallNs = 0;
  if (const json::Object *Meta = Root->getObject("metadata")) {
    InputFile = Meta->getString("input_file").value_or("unknown").str();
    TargetTriple = Meta->getString("target_triple").value_or("unknown").str();
    TotalWallNs = Meta->getInteger("total_wall_time_ns").value_or(0);
  }

  // Parse Diffs Map
  std::map<std::string, std::string> DiffsMap;
  if (const json::Object *DiffsObj = Root->getObject("diffs")) {
    for (const auto &KV : *DiffsObj) {
      if (auto Str = KV.second.getAsString())
        DiffsMap[KV.first.str()] = Str->str();
    }
  }

  // Parse Invocations
  std::vector<InvDetail> Invocations;
  uint64_t SumSelfNs = 0;
  size_t MutatedCount = 0;
  size_t UnderReportCount = 0;

  if (const json::Array *Invs = Root->getArray("invocations")) {
    for (const auto &Val : *Invs) {
      const json::Object *Obj = Val.getAsObject();
      if (!Obj)
        continue;

      InvDetail Inv;
      Inv.Id = Obj->getInteger("id").value_or(0);
      Inv.PassID = Obj->getString("pass_id").value_or("").str();
      Inv.Depth = Obj->getInteger("depth").value_or(0);
      Inv.DiffId = Obj->getString("diff_id").value_or("").str();
      if (!Inv.DiffId.empty()) {
        auto It = DiffsMap.find(Inv.DiffId);
        if (It != DiffsMap.end())
          Inv.Diff = It->second;
      }

      if (const json::Object *Unit = Obj->getObject("unit")) {
        Inv.UnitKind = Unit->getString("kind").value_or("").str();
        Inv.UnitName = Unit->getString("name").value_or("").str();
      }

      if (const json::Object *Timing = Obj->getObject("timing")) {
        Inv.SelfNs = Timing->getInteger("self_ns").value_or(0);
        Inv.TotalNs = Timing->getInteger("total_ns").value_or(0);
      }
      SumSelfNs += Inv.SelfNs;

      if (const json::Object *Change = Obj->getObject("change")) {
        Inv.Changed = Change->getBoolean("changed").value_or(false);
        Inv.UnderReporting =
            Change->getBoolean("under_reporting").value_or(false);
        Inv.BeforeHash = Change->getString("before_hash").value_or("").str();
        Inv.AfterHash = Change->getString("after_hash").value_or("").str();
      }

      if (Inv.Changed)
        ++MutatedCount;
      if (Inv.UnderReporting)
        ++UnderReportCount;

      if (const json::Object *Sig = Obj->getObject("significance")) {
        if (const json::Object *Struct = Sig->getObject("structural")) {
          Inv.DeltaInsts = Struct->getInteger("delta_instructions").value_or(0);
          Inv.DeltaBBs = Struct->getInteger("delta_basic_blocks").value_or(0);
        }
        if (const json::Object *Mem = Sig->getObject("memory")) {
          Inv.DeltaLoads = Mem->getInteger("delta_loads").value_or(0);
          Inv.DeltaStores = Mem->getInteger("delta_stores").value_or(0);
          Inv.DeltaAllocas = Mem->getInteger("delta_allocas").value_or(0);
        }
        if (const json::Object *CG = Sig->getObject("callgraph")) {
          Inv.DeltaCalls = CG->getInteger("delta_direct_calls").value_or(0) +
                           CG->getInteger("delta_indirect_calls").value_or(0);
          Inv.InlinedCalls = CG->getInteger("inlined_functions").value_or(0);
        }
        Inv.VectorWidth = Sig->getInteger("max_vector_width").value_or(1);
        if (const json::Object *Cost = Sig->getObject("cost")) {
          Inv.TTICostDelta = Cost->getInteger("tti_cost_delta").value_or(0);
          Inv.WeightedCostDelta =
              Cost->getNumber("weighted_hotpath_cost_delta").value_or(0.0);
        }
      }

      Invocations.push_back(std::move(Inv));
    }
  }

  // Parse Remarks
  std::vector<RemarkDetail> Remarks;
  if (const json::Array *RemArr = Root->getArray("remarks")) {
    for (const auto &Val : *RemArr) {
      const json::Object *Obj = Val.getAsObject();
      if (!Obj)
        continue;
      RemarkDetail R;
      R.Id = Obj->getInteger("id").value_or(0);
      R.PassName = Obj->getString("pass_name").value_or("").str();
      R.Function = Obj->getString("function").value_or("").str();
      R.Message = Obj->getString("message").value_or("").str();
      R.File = Obj->getString("file").value_or("").str();
      R.Line = Obj->getInteger("line").value_or(0);
      R.Column = Obj->getInteger("column").value_or(0);
      Remarks.push_back(std::move(R));
    }
  }

  // Parse Ablation
  int64_t BaselineSize = 0;
  int64_t BaselineInsts = 0;
  std::vector<AblateCandidateDetail> AblateCandidates;
  if (const json::Object *Abl = Root->getObject("ablation")) {
    if (const json::Object *Base = Abl->getObject("baseline")) {
      BaselineSize = Base->getInteger("object_size_bytes").value_or(0);
      BaselineInsts = Base->getInteger("machine_instructions").value_or(0);
    }
    if (const json::Array *Cands = Abl->getArray("candidates")) {
      for (const auto &Val : *Cands) {
        const json::Object *Obj = Val.getAsObject();
        if (!Obj)
          continue;
        AblateCandidateDetail C;
        C.PassID = Obj->getString("pass_id").value_or("").str();
        C.AblatedSize = Obj->getInteger("ablated_size_bytes").value_or(0);
        C.AblatedInsts =
            Obj->getInteger("ablated_machine_instructions").value_or(0);
        C.DeltaSize = Obj->getInteger("delta_size_bytes").value_or(0);
        C.DeltaInsts =
            Obj->getInteger("delta_machine_instructions").value_or(0);
        C.Status = Obj->getString("status").value_or("").str();
        AblateCandidates.push_back(std::move(C));
      }
    }
  }

  std::string ReportStr;
  raw_string_ostream OS(ReportStr);

  OS << "================================================================================\n";
  OS << "                        VERNIER PIPELINE PROFILE REPORT                         \n";
  OS << "================================================================================\n";
  OS << formatv("Input Source      : {0}\n", InputFile);
  OS << formatv("Target Triple     : {0}\n", TargetTriple);
  OS << formatv("Total Wall Time   : {0} ({1} accounted self time)\n",
                formatDuration(TotalWallNs), formatDuration(SumSelfNs));
  OS << formatv("Pass Invocations  : {0} total (IR Mutated: {1}, Under-reported PA: {2})\n",
                Invocations.size(), MutatedCount, UnderReportCount);
  OS << formatv("Attributed Remarks: {0}\n\n", Remarks.size());

  // Section 1: Top Hotspots
  OS << "--------------------------------------------------------------------------------\n";
  OS << "1. TOP HOTSPOTS (BY PASS SELF TIME)\n";
  OS << "--------------------------------------------------------------------------------\n";
  OS << "Rank  Self Time    % Total    Activity Bar      Pass ID (Target)\n";
  OS << "--------------------------------------------------------------------------------\n";

  std::vector<const InvDetail *> Hotspots;
  for (const auto &Inv : Invocations)
    Hotspots.push_back(&Inv);

  std::sort(Hotspots.begin(), Hotspots.end(),
            [](const InvDetail *A, const InvDetail *B) {
              return A->SelfNs > B->SelfNs;
            });

  size_t MaxHotspots = std::min(size_t(10), Hotspots.size());
  for (size_t i = 0; i < MaxHotspots; ++i) {
    const auto *Inv = Hotspots[i];
    double Pct = SumSelfNs > 0
                     ? (static_cast<double>(Inv->SelfNs) / SumSelfNs) * 100.0
                     : 0.0;
    std::string Bar =
        makeProgressBar(SumSelfNs > 0 ? (double)Inv->SelfNs / SumSelfNs : 0.0);
    std::string Target = Inv->UnitName.empty()
                             ? Inv->UnitKind
                             : (Inv->UnitName + " [" + Inv->UnitKind + "]");

    std::ostringstream LineSS;
    LineSS << std::setw(3) << (i + 1) << ".  " << std::setw(10)
           << formatDuration(Inv->SelfNs) << "   " << std::setw(5) << std::fixed
           << std::setprecision(1) << Pct << "%   " << Bar << "   "
           << Inv->PassID << " (" << Target << ")\n";
    OS << LineSS.str();
  }
  OS << "\n";

  // Section 2: Optimization Significance
  OS << "--------------------------------------------------------------------------------\n";
  OS << "2. OPTIMIZATION SIGNIFICANCE (TOP IMPACT PASSES)\n";
  OS << "--------------------------------------------------------------------------------\n";
  OS << "Pass ID                    Target               dInst  dBB  dMem  dCall  Vec  TTI Cost\n";
  OS << "--------------------------------------------------------------------------------\n";

  std::vector<const InvDetail *> SigRanked;
  for (const auto &Inv : Invocations) {
    if (Inv.Changed || Inv.DeltaInsts != 0 || Inv.TTICostDelta != 0)
      SigRanked.push_back(&Inv);
  }

  std::sort(SigRanked.begin(), SigRanked.end(),
            [](const InvDetail *A, const InvDetail *B) {
              int64_t ScoreA = std::abs(A->DeltaInsts) * 3 +
                               std::abs(A->DeltaBBs) * 5 +
                               std::abs(A->DeltaLoads + A->DeltaStores) * 4 +
                               std::abs(A->TTICostDelta);
              int64_t ScoreB = std::abs(B->DeltaInsts) * 3 +
                               std::abs(B->DeltaBBs) * 5 +
                               std::abs(B->DeltaLoads + B->DeltaStores) * 4 +
                               std::abs(B->TTICostDelta);
              return ScoreA > ScoreB;
            });

  size_t MaxSig = std::min(size_t(12), SigRanked.size());
  if (MaxSig == 0) {
    OS << "  (No IR modifications or structural deltas recorded)\n";
  } else {
    for (size_t i = 0; i < MaxSig; ++i) {
      const auto *Inv = SigRanked[i];
      int64_t dMem = Inv->DeltaLoads + Inv->DeltaStores + Inv->DeltaAllocas;

      std::string PassTrunc = Inv->PassID;
      if (PassTrunc.size() > 25)
        PassTrunc = PassTrunc.substr(0, 23) + "..";

      std::string TargetTrunc = Inv->UnitName;
      if (TargetTrunc.size() > 18)
        TargetTrunc = TargetTrunc.substr(0, 16) + "..";

      std::ostringstream LineSS;
      LineSS << std::left << std::setw(26) << PassTrunc << std::setw(19)
             << TargetTrunc << std::right << std::setw(7)
             << formatSigned(Inv->DeltaInsts) << std::setw(5)
             << formatSigned(Inv->DeltaBBs) << std::setw(6)
             << formatSigned(dMem) << std::setw(7)
             << formatSigned(Inv->DeltaCalls) << std::setw(5)
             << Inv->VectorWidth << std::setw(10)
             << formatSigned(Inv->TTICostDelta) << "\n";
      OS << LineSS.str();
    }
  }
  OS << "\n";

  // Section 3: Remarks
  if (!Remarks.empty()) {
    OS << "--------------------------------------------------------------------------------\n";
    OS << "3. REMARKS & MISSED OPTIMIZATIONS\n";
    OS << "--------------------------------------------------------------------------------\n";
    for (size_t i = 0; i < Remarks.size(); ++i) {
      const auto &R = Remarks[i];
      OS << formatv("[#{0}] Pass: {1}\n", i + 1, R.PassName);
      if (!R.File.empty() || R.Line > 0)
        OS << formatv("     Location: {0}:{1}:{2}\n", R.File, R.Line, R.Column);
      if (!R.Function.empty())
        OS << formatv("     Function: {0}\n", R.Function);
      OS << formatv("     Message : {0}\n\n", R.Message);
    }
  }

  // Section 4: Codegen Ablation
  if (!AblateCandidates.empty()) {
    OS << "--------------------------------------------------------------------------------\n";
    OS << "4. CODEGEN ABLATION IMPACT\n";
    OS << "--------------------------------------------------------------------------------\n";
    OS << "Pass ID                    Obj Size (B)   Delta (B)   Mach Insts   Delta Insts\n";
    OS << "--------------------------------------------------------------------------------\n";
    std::ostringstream BaseSS;
    BaseSS << std::left << std::setw(26) << "(Baseline)" << std::right
           << std::setw(13) << BaselineSize << std::setw(12) << "+0"
           << std::setw(13) << BaselineInsts << std::setw(14) << "+0\n";
    OS << BaseSS.str();

    for (const auto &C : AblateCandidates) {
      std::string PassTrunc = C.PassID;
      if (PassTrunc.size() > 25)
        PassTrunc = PassTrunc.substr(0, 23) + "..";

      std::ostringstream CandSS;
      CandSS << std::left << std::setw(26) << PassTrunc << std::right
             << std::setw(13) << C.AblatedSize << std::setw(12)
             << formatSigned(C.DeltaSize) << std::setw(13) << C.AblatedInsts
             << std::setw(14) << formatSigned(C.DeltaInsts) << "\n";
      OS << CandSS.str();
    }
    OS << "\n";
  }

  // Section 5: Pipeline Execution Tree
  OS << "--------------------------------------------------------------------------------\n";
  OS << "5. PIPELINE EXECUTION TREE\n";
  OS << "--------------------------------------------------------------------------------\n";
  OS << "Depth  Self Time   Status  Pass ID & Target\n";
  OS << "--------------------------------------------------------------------------------\n";

  for (const auto &Inv : Invocations) {
    std::string Indent(Inv.Depth * 2, ' ');
    std::string Status = Inv.Changed ? "[*]" : "[ ]";
    if (Inv.UnderReporting)
      Status = "[!]";

    std::string Target = Inv.UnitName.empty() ? "" : (" (" + Inv.UnitName + ")");
    std::string Extra = "";
    if (Inv.DeltaInsts != 0)
      Extra += " [dInst: " + formatSigned(Inv.DeltaInsts) + "]";
    if (Inv.VectorWidth > 1)
      Extra += " [SIMD: " + std::to_string(Inv.VectorWidth) + "x]";

    std::ostringstream TreeSS;
    TreeSS << "[" << Inv.Depth << "]   " << std::right << std::setw(10)
           << formatDuration(Inv.SelfNs) << "    " << Status << "   " << Indent
           << Inv.PassID << Target << Extra << "\n";
    OS << TreeSS.str();
  }
  OS << "\n";

  // Section 6: Unified Diffs
  size_t DiffCount = 0;
  for (const auto &Inv : Invocations) {
    if (!Inv.Diff.empty())
      ++DiffCount;
  }

  if (DiffCount > 0) {
    OS << "--------------------------------------------------------------------------------\n";
    OS << formatv("6. UNIFIED IR MODIFICATION DIFFS ({0} Recorded Mutations)\n",
                  DiffCount);
    OS << "--------------------------------------------------------------------------------\n";
    for (const auto &Inv : Invocations) {
      if (Inv.Diff.empty())
        continue;
      OS << formatv(">>> Invocation #{0}: {1} on {2} ({3})\n", Inv.Id,
                    Inv.PassID, Inv.UnitName, Inv.UnitKind);
      OS << formatv("    Hashes: {0} -> {1}\n", Inv.BeforeHash, Inv.AfterHash);
      OS << Inv.Diff;
      if (!Inv.Diff.empty() && Inv.Diff.back() != '\n')
        OS << "\n";
      OS << "\n";
    }
  }

  OS << "================================================================================\n";
  OS << "                           END OF VERNIER REPORT                                \n";
  OS << "================================================================================\n";

  outs() << ReportStr;

  if (!OutPath.empty()) {
    std::error_code EC;
    raw_fd_ostream FileOS(OutPath, EC, sys::fs::OF_None);
    if (!EC) {
      FileOS << ReportStr;
      FileOS.flush();
      outs() << "Report written to " << OutPath << "\n";
    } else {
      errs() << "Vernier: Failed to write output file '" << OutPath
             << "': " << EC.message() << "\n";
    }
  }

  return 0;
}

} // namespace vernier
