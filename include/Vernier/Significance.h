#ifndef VERNIER_SIGNIFICANCE_H
#define VERNIER_SIGNIFICANCE_H

#include "Vernier/Schema.h"
#include "Vernier/support.h"
#include "llvm/IR/PassManager.h"

namespace vernier {

struct IRMetrics {
  int64_t Instructions = 0;
  int64_t BasicBlocks = 0;
  int64_t CFGEdges = 0;
  int64_t Loops = 0;

  int64_t Loads = 0;
  int64_t Stores = 0;
  int64_t Allocas = 0;

  int64_t DirectCalls = 0;
  int64_t IndirectCalls = 0;
  int64_t InlinedFunctions = 0;

  uint32_t MaxVectorWidth = 1;

  double TTICost = 0.0;
  double WeightedHotpathCost = 0.0;
};

IRMetrics computeMetrics(const IRUnit &Unit,
                         llvm::ModuleAnalysisManager *MAM = nullptr);

schema::SignificanceInfo computeSignificanceDelta(const IRMetrics &Before,
                                                 const IRMetrics &After);

} // namespace vernier

#endif // VERNIER_SIGNIFICANCE_H
