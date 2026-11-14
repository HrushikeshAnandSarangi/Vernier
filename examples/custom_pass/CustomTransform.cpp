#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// CustomTransformPass: A sample custom IR transformation pass.
// Scans for binary addition instructions with a constant operand
// and canonicalizes them to equivalent subtraction (e.g. x + C => x - (-C))
// to demonstrate an IR mutation visible in Vernier diffs and tracing.
struct CustomTransformPass : public PassInfoMixin<CustomTransformPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;

    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
          if (BO->getOpcode() == Instruction::Add) {
            // Check if operand 1 is a constant integer
            if (auto *CI = dyn_cast<ConstantInt>(BO->getOperand(1))) {
              if (CI->getBitWidth() <= 64 && !CI->isZero()) {
                // Mutate add to sub with negated constant
                APInt NegVal = -CI->getValue();
                Constant *NegConst = ConstantInt::get(CI->getContext(), NegVal);
                auto *NewSub = BinaryOperator::CreateSub(
                    BO->getOperand(0), NegConst, BO->getName() + ".sub", BO);
                BO->replaceAllUsesWith(NewSub);
                Changed = true;
                break;
              }
            }
          }
        }
      }
    }

    if (!Changed)
      return PreservedAnalyses::all();

    PreservedAnalyses PA;
    PA.preserveSet<CFGAnalyses>();
    return PA;
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "CustomTransform", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [](StringRef Name, FunctionPassManager &FPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "custom-transform") {
                FPM.addPass(CustomTransformPass());
                return true;
              }
              return false;
            });
      }};
}
