#include "Vernier/support.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
int Depth = 0;
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Vernier", LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            auto *PIC = PB.getPassInstrumentationCallbacks();
            if (!PIC)
              return;

            PIC->registerBeforeNonSkippedPassCallback(
                [](StringRef PassID, Any IR) {
                  vernier::IRUnit U = vernier::describe(IR);
                  errs().indent(Depth * 2)
                      << "> " << PassID << " (" << vernier::KindName(U.K)
                      << ": " << U.Name << ")\n";
                  ++Depth;
                });

            PIC->registerAfterPassCallback(
                [](StringRef /*PassID*/, Any /*IR*/, const PreservedAnalyses &/*PA*/) {
                  if (Depth > 0)
                    --Depth;
                });

            llvm::errs() << "Tool Loaded, PassBuilder Attached\n";
          }};
}
