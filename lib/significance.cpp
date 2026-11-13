#include "Vernier/Significance.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"

#include <algorithm>

using namespace llvm;

namespace vernier {

static void analyzeFunction(const Function &F, IRMetrics &M,
                            BlockFrequencyInfo *BFI, TargetTransformInfo *TTI) {
  if (F.isDeclaration())
    return;

  M.BasicBlocks += F.size();

  double EntryFreq = 1.0;
  if (BFI && BFI->getEntryFreq().getFrequency() > 0)
    EntryFreq = static_cast<double>(BFI->getEntryFreq().getFrequency());

  for (const BasicBlock &BB : F) {
    if (const Instruction *Term = BB.getTerminator())
      M.CFGEdges += Term->getNumSuccessors();

    double Freq = 1.0;
    if (BFI)
      Freq = static_cast<double>(BFI->getBlockFreq(&BB).getFrequency()) / EntryFreq;

    for (const Instruction &I : BB) {
      ++M.Instructions;

      if (isa<LoadInst>(I))
        ++M.Loads;
      else if (isa<StoreInst>(I))
        ++M.Stores;
      else if (isa<AllocaInst>(I))
        ++M.Allocas;

      if (const auto *CB = dyn_cast<CallBase>(&I)) {
        if (CB->getCalledFunction())
          ++M.DirectCalls;
        else
          ++M.IndirectCalls;
      }

      if (const auto *VTy = dyn_cast<VectorType>(I.getType())) {
        uint32_t Elements = VTy->getElementCount().getKnownMinValue();
        M.MaxVectorWidth = std::max(M.MaxVectorWidth, Elements);
      }
      for (const auto &Op : I.operands()) {
        if (const auto *VTy = dyn_cast<VectorType>(Op->getType())) {
          uint32_t Elements = VTy->getElementCount().getKnownMinValue();
          M.MaxVectorWidth = std::max(M.MaxVectorWidth, Elements);
        }
      }

      double InstCost = 1.0;
      if (TTI) {
        InstructionCost Cost =
            TTI->getInstructionCost(&I, TargetTransformInfo::TCK_SizeAndLatency);
        if (Cost.isValid()) {
          auto Val = Cost.getValue();
          if (Val.has_value())
            InstCost = static_cast<double>(*Val);
        }
      }

      M.TTICost += InstCost;
      M.WeightedHotpathCost += (InstCost * Freq);
    }
  }
}

IRMetrics computeMetrics(const IRUnit &Unit, ModuleAnalysisManager *MAM) {
  IRMetrics M;

  auto getAnalyses = [&](const Function &F, BlockFrequencyInfo *&BFI,
                         TargetTransformInfo *&TTI) {
    BFI = nullptr;
    TTI = nullptr;
    if (!MAM || !F.getParent())
      return;
    Module *NonConstM = const_cast<Module *>(F.getParent());
    if (auto *Proxy = MAM->getCachedResult<FunctionAnalysisManagerModuleProxy>(*NonConstM)) {
      FunctionAnalysisManager &FAM = Proxy->getManager();
      Function &NonConstF = const_cast<Function &>(F);
      BFI = FAM.getCachedResult<BlockFrequencyAnalysis>(NonConstF);
      TTI = FAM.getCachedResult<TargetIRAnalysis>(NonConstF);
    }
  };

  if (Unit.F) {
    BlockFrequencyInfo *BFI = nullptr;
    TargetTransformInfo *TTI = nullptr;
    getAnalyses(*Unit.F, BFI, TTI);
    analyzeFunction(*Unit.F, M, BFI, TTI);
  } else if (Unit.M) {
    for (const Function &F : *Unit.M) {
      if (!F.isDeclaration()) {
        BlockFrequencyInfo *BFI = nullptr;
        TargetTransformInfo *TTI = nullptr;
        getAnalyses(F, BFI, TTI);
        analyzeFunction(F, M, BFI, TTI);
      }
    }
  } else if (Unit.L && Unit.F) {
    BlockFrequencyInfo *BFI = nullptr;
    TargetTransformInfo *TTI = nullptr;
    getAnalyses(*Unit.F, BFI, TTI);
    analyzeFunction(*Unit.F, M, BFI, TTI);
  }

  return M;
}

schema::SignificanceInfo computeSignificanceDelta(const IRMetrics &Before,
                                                 const IRMetrics &After) {
  schema::SignificanceInfo Sig;
  Sig.Structural.DeltaInstructions = After.Instructions - Before.Instructions;
  Sig.Structural.DeltaBasicBlocks = After.BasicBlocks - Before.BasicBlocks;
  Sig.Structural.DeltaCFGEdges = After.CFGEdges - Before.CFGEdges;
  Sig.Structural.DeltaLoops = After.Loops - Before.Loops;

  Sig.Memory.DeltaLoads = After.Loads - Before.Loads;
  Sig.Memory.DeltaStores = After.Stores - Before.Stores;
  Sig.Memory.DeltaAllocas = After.Allocas - Before.Allocas;

  Sig.CallGraph.DeltaDirectCalls = After.DirectCalls - Before.DirectCalls;
  Sig.CallGraph.DeltaIndirectCalls = After.IndirectCalls - Before.IndirectCalls;
  Sig.CallGraph.InlinedFunctions = After.InlinedFunctions - Before.InlinedFunctions;

  Sig.MaxVectorWidth = After.MaxVectorWidth;

  Sig.Cost.TTICostDelta = After.TTICost - Before.TTICost;
  Sig.Cost.WeightedHotpathCostDelta =
      After.WeightedHotpathCost - Before.WeightedHotpathCost;

  return Sig;
}

} // namespace vernier
