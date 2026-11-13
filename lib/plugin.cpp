#include "Vernier/Session.h"
#include "Vernier/Trace.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
class VernierAnalysisAnchorPass
    : public PassInfoMixin<VernierAnalysisAnchorPass> {
public:
  PreservedAnalyses run(Module &/*M*/, ModuleAnalysisManager &MAM) {
    vernier::Session::getInstance().setAnalysisManager(&MAM);
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Vernier", LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            auto *PIC = PB.getPassInstrumentationCallbacks();
            if (PIC) {
              vernier::registerVernierCallbacks(*PIC);
            }

            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel /*Level*/) {
                  MPM.addPass(VernierAnalysisAnchorPass());
                });
          }};
}
