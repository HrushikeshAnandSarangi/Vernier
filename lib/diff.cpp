#include "Vernier/Diff.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace llvm;

namespace vernier {

std::string computeUnifiedDiff(StringRef Name, StringRef BeforeText,
                               StringRef AfterText, const DiffOptions &Opts) {
  if (BeforeText == AfterText)
    return "";

  SmallVector<StringRef, 128> BeforeLines;
  SmallVector<StringRef, 128> AfterLines;
  BeforeText.split(BeforeLines, '\n');
  AfterText.split(AfterLines, '\n');

  // Strip empty trailing line if present
  if (!BeforeLines.empty() && BeforeLines.back().empty())
    BeforeLines.pop_back();
  if (!AfterLines.empty() && AfterLines.back().empty())
    AfterLines.pop_back();

  size_t Prefix = 0;
  while (Prefix < BeforeLines.size() && Prefix < AfterLines.size() &&
         BeforeLines[Prefix] == AfterLines[Prefix]) {
    ++Prefix;
  }

  size_t Suffix = 0;
  while (Suffix + Prefix < BeforeLines.size() &&
         Suffix + Prefix < AfterLines.size() &&
         BeforeLines[BeforeLines.size() - 1 - Suffix] ==
             AfterLines[AfterLines.size() - 1 - Suffix]) {
    ++Suffix;
  }

  size_t WindowBefore = BeforeLines.size() - Suffix - Prefix;
  size_t WindowAfter = AfterLines.size() - Suffix - Prefix;

  size_t CtxBefore = std::min(Prefix, Opts.ContextLines);
  size_t CtxAfter = std::min(Suffix, Opts.ContextLines);

  size_t BeforeHunkStart = Prefix - CtxBefore;
  size_t BeforeHunkEnd = BeforeLines.size() - Suffix + CtxAfter;
  size_t AfterHunkStart = Prefix - CtxBefore;
  size_t AfterHunkEnd = AfterLines.size() - Suffix + CtxAfter;

  size_t BeforeHunkCount = BeforeHunkEnd - BeforeHunkStart;
  size_t AfterHunkCount = AfterHunkEnd - AfterHunkStart;

  std::string Result;
  raw_string_ostream OS(Result);

  OS << "--- " << Name << " (before)\n";
  OS << "+++ " << Name << " (after)\n";
  OS << "@@ -" << (BeforeHunkStart + 1) << "," << BeforeHunkCount << " +"
     << (AfterHunkStart + 1) << "," << AfterHunkCount << " @@\n";

  // Pre-context lines
  for (size_t i = BeforeHunkStart; i < Prefix; ++i) {
    OS << " " << BeforeLines[i] << "\n";
  }

  // Size cap fallback
  if (WindowBefore + WindowAfter > Opts.MaxDiffLines) {
    OS << " [Diff truncated: -" << WindowBefore << " lines, +" << WindowAfter
       << " lines (exceeds " << Opts.MaxDiffLines << " max diff threshold)]\n";
  } else {
    // Standard LCS for the modified window
    size_t M = WindowBefore;
    size_t N = WindowAfter;
    std::vector<std::vector<int>> DP(M + 1, std::vector<int>(N + 1, 0));

    for (size_t i = 1; i <= M; ++i) {
      for (size_t j = 1; j <= N; ++j) {
        if (BeforeLines[Prefix + i - 1] == AfterLines[Prefix + j - 1])
          DP[i][j] = DP[i - 1][j - 1] + 1;
        else
          DP[i][j] = std::max(DP[i - 1][j], DP[i][j - 1]);
      }
    }

    // Backtrack to build diff ops
    struct DiffOp {
      char Type; // '-', '+', ' '
      StringRef Line;
    };
    std::vector<DiffOp> Ops;

    size_t i = M, j = N;
    while (i > 0 || j > 0) {
      if (i > 0 && j > 0 &&
          BeforeLines[Prefix + i - 1] == AfterLines[Prefix + j - 1]) {
        Ops.push_back({' ', BeforeLines[Prefix + i - 1]});
        --i;
        --j;
      } else if (j > 0 && (i == 0 || DP[i][j - 1] >= DP[i - 1][j])) {
        Ops.push_back({'+', AfterLines[Prefix + j - 1]});
        --j;
      } else if (i > 0 && (j == 0 || DP[i][j - 1] < DP[i - 1][j])) {
        Ops.push_back({'-', BeforeLines[Prefix + i - 1]});
        --i;
      }
    }
    std::reverse(Ops.begin(), Ops.end());

    for (const auto &Op : Ops) {
      OS << Op.Type << " " << Op.Line << "\n";
    }
  }

  // Post-context lines
  size_t PostStart = BeforeLines.size() - Suffix;
  for (size_t i = PostStart; i < BeforeHunkEnd; ++i) {
    OS << " " << BeforeLines[i] << "\n";
  }

  return Result;
}

} // namespace vernier
