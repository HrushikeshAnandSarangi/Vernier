#include "compare.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <iomanip>
#include <vector>

using namespace llvm;

namespace vernier {

struct InvSummary {
  uint64_t Id = 0;
  std::string PassID;
  std::string UnitKind;
  std::string UnitName;
  std::string AfterHash;
  uint64_t SelfNs = 0;
  uint64_t TotalNs = 0;
  bool Changed = false;
  int64_t DeltaInstructions = 0;
};

static std::vector<InvSummary> parseInvocations(const json::Object &Root) {
  std::vector<InvSummary> Result;
  const json::Array *Invs = Root.getArray("invocations");
  if (!Invs)
    return Result;

  for (const auto &Val : *Invs) {
    const json::Object *Obj = Val.getAsObject();
    if (!Obj)
      continue;

    InvSummary Inv;
    Inv.Id = Obj->getInteger("id").value_or(0);
    Inv.PassID = Obj->getString("pass_id").value_or("").str();

    if (const json::Object *Unit = Obj->getObject("unit")) {
      Inv.UnitKind = Unit->getString("kind").value_or("").str();
      Inv.UnitName = Unit->getString("name").value_or("").str();
    }

    if (const json::Object *Timing = Obj->getObject("timing")) {
      Inv.SelfNs = Timing->getInteger("self_ns").value_or(0);
      Inv.TotalNs = Timing->getInteger("total_ns").value_or(0);
    }

    if (const json::Object *Change = Obj->getObject("change")) {
      Inv.Changed = Change->getBoolean("changed").value_or(false);
      Inv.AfterHash = Change->getString("after_hash").value_or("").str();
    }

    if (const json::Object *Sig = Obj->getObject("significance")) {
      if (const json::Object *Struct = Sig->getObject("structural")) {
        Inv.DeltaInstructions =
            Struct->getInteger("delta_instructions").value_or(0);
      }
    }

    Result.push_back(std::move(Inv));
  }

  return Result;
}

static std::string formatSigned(int64_t V) {
  if (V >= 0)
    return "+" + std::to_string(V);
  return std::to_string(V);
}

int runCompare(const std::string &JsonPathA, const std::string &JsonPathB,
               const std::string &OutPath) {
  auto BufA = MemoryBuffer::getFile(JsonPathA);
  if (!BufA) {
    errs() << "Vernier: Failed to read '" << JsonPathA << "': "
           << BufA.getError().message() << "\n";
    return 1;
  }

  auto BufB = MemoryBuffer::getFile(JsonPathB);
  if (!BufB) {
    errs() << "Vernier: Failed to read '" << JsonPathB << "': "
           << BufB.getError().message() << "\n";
    return 1;
  }

  auto ParsedA = json::parse((*BufA)->getBuffer());
  auto ParsedB = json::parse((*BufB)->getBuffer());
  if (!ParsedA || !ParsedB) {
    errs() << "Vernier: Failed to parse input JSON files\n";
    return 1;
  }

  const json::Object *RootA = ParsedA->getAsObject();
  const json::Object *RootB = ParsedB->getAsObject();
  if (!RootA || !RootB) {
    errs() << "Vernier: Invalid root JSON\n";
    return 1;
  }

  std::vector<InvSummary> SeqA = parseInvocations(*RootA);
  std::vector<InvSummary> SeqB = parseInvocations(*RootB);

  size_t M = SeqA.size();
  size_t N = SeqB.size();

  outs() << formatv("Vernier: Aligning Run A ({0} passes) vs Run B ({1} passes)...\n",
                    M, N);

  // Needleman-Wunsch Global Sequence Alignment
  const int GapPenalty = -2;
  auto score = [](const InvSummary &A, const InvSummary &B) -> int {
    if (A.PassID == B.PassID) {
      if (A.UnitKind == B.UnitKind && A.UnitName == B.UnitName)
        return 2;
      return 1;
    }
    return -1;
  };

  std::vector<std::vector<int>> DP(M + 1, std::vector<int>(N + 1, 0));
  for (size_t i = 0; i <= M; ++i)
    DP[i][0] = static_cast<int>(i) * GapPenalty;
  for (size_t j = 0; j <= N; ++j)
    DP[0][j] = static_cast<int>(j) * GapPenalty;

  for (size_t i = 1; i <= M; ++i) {
    for (size_t j = 1; j <= N; ++j) {
      int Match = DP[i - 1][j - 1] + score(SeqA[i - 1], SeqB[j - 1]);
      int DelA = DP[i - 1][j] + GapPenalty;
      int InsB = DP[i][j - 1] + GapPenalty;
      DP[i][j] = std::max({Match, DelA, InsB});
    }
  }

  // Traceback
  std::vector<std::pair<const InvSummary *, const InvSummary *>> Aligned;
  size_t i = M, j = N;
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0 &&
        DP[i][j] == DP[i - 1][j - 1] + score(SeqA[i - 1], SeqB[j - 1])) {
      Aligned.push_back({&SeqA[i - 1], &SeqB[j - 1]});
      --i;
      --j;
    } else if (i > 0 && DP[i][j] == DP[i - 1][j] + GapPenalty) {
      Aligned.push_back({&SeqA[i - 1], nullptr});
      --i;
    } else if (j > 0 && DP[i][j] == DP[i][j - 1] + GapPenalty) {
      Aligned.push_back({nullptr, &SeqB[j - 1]});
      --j;
    } else {
      break;
    }
  }
  std::reverse(Aligned.begin(), Aligned.end());

  // Walk alignment to find first function hash divergence
  size_t ExactMatches = 0;
  size_t GapsA = 0;
  size_t GapsB = 0;
  const InvSummary *DivA = nullptr;
  const InvSummary *DivB = nullptr;
  size_t DivStep = 0;

  for (size_t step = 0; step < Aligned.size(); ++step) {
    const auto *A = Aligned[step].first;
    const auto *B = Aligned[step].second;

    if (!A) {
      ++GapsA;
      continue;
    }
    if (!B) {
      ++GapsB;
      continue;
    }

    if (A->PassID == B->PassID)
      ++ExactMatches;

    if (!DivA && A->UnitName == B->UnitName && !A->AfterHash.empty() &&
        !B->AfterHash.empty() && A->AfterHash != B->AfterHash) {
      DivA = A;
      DivB = B;
      DivStep = step;
    }
  }

  std::string ReportStr;
  raw_string_ostream OS(ReportStr);

  OS << "=================================================================\n";
  OS << "               VERNIER PIPELINE DIVERGENCE REPORT                \n";
  OS << "=================================================================\n";
  OS << formatv("Run A: {0} ({1} invocations)\n", JsonPathA, M);
  OS << formatv("Run B: {0} ({1} invocations)\n", JsonPathB, N);
  OS << formatv("Alignment Length: {0} steps (Exact Pass Matches: {1}, Gaps: A={2}, B={3})\n\n",
                Aligned.size(), ExactMatches, GapsA, GapsB);

  if (DivA && DivB) {
    OS << "-----------------------------------------------------------------\n";
    OS << "                      FIRST DIVERGENCE POINT                     \n";
    OS << "-----------------------------------------------------------------\n";
    OS << formatv("Alignment Step   : #{0}\n", DivStep);
    OS << formatv("Pass ID          : {0}\n", DivA->PassID);
    OS << formatv("IR Unit          : {0} ({1})\n", DivA->UnitName, DivA->UnitKind);
    OS << "\nRun A State:\n";
    OS << formatv("  Invocation ID  : #{0}\n", DivA->Id);
    OS << formatv("  IR Hash (After): {0}\n", DivA->AfterHash);
    OS << formatv("  Self Time      : {0} ns\n", DivA->SelfNs);
    OS << formatv("  Delta Insts    : {0}\n", formatSigned(DivA->DeltaInstructions));

    OS << "\nRun B State:\n";
    OS << formatv("  Invocation ID  : #{0}\n", DivB->Id);
    OS << formatv("  IR Hash (After): {0}\n", DivB->AfterHash);
    OS << formatv("  Self Time      : {0} ns\n", DivB->SelfNs);
    OS << formatv("  Delta Insts    : {0}\n", formatSigned(DivB->DeltaInstructions));

    int64_t TimeDelta = static_cast<int64_t>(DivB->SelfNs) -
                        static_cast<int64_t>(DivA->SelfNs);
    OS << formatv("\nTiming Delta (B - A): {0} ns\n", formatSigned(TimeDelta));
  } else {
    OS << "No IR hash divergence detected across all aligned passes!\n";
  }
  OS << "=================================================================\n";

  outs() << ReportStr;

  if (!OutPath.empty()) {
    std::error_code EC;
    raw_fd_ostream FileOS(OutPath, EC, sys::fs::OF_None);
    if (!EC) {
      FileOS << ReportStr;
      FileOS.flush();
      outs() << "Divergence report written to " << OutPath << "\n";
    }
  }

  return 0;
}

} // namespace vernier
