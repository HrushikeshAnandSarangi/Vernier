#include "Vernier/Trace.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Vernier", LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            auto *PIC = PB.getPassInstrumentationCallbacks();
            if (!PIC)
              return;

            vernier::registerVernierCallbacks(*PIC);
          }};
}
