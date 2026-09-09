//Author:@HrushikeshAnandSarangi
//This is main file for llvm pass analyzer plugin

#include "llvm/Config/llvm-config.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"

using namespace llvm;
namespace{
int Depth=0;
}



extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(){
  return {LLVM_PLUGIN_API_VERSION,"Vernier",LLVM_VERSION_STRING,[](llvm::PassBuilder &PB){
    auto *PIC=PB.getPassInstrumentationCallbacks();
    if(!PIC)
      return;
    PIC->registerBeforeNonSkippedPassCallback([](StringRef PassID, Any IR){
      errs().indent(Depth*2)<<"> "<<PassID<<"\n";
      ++Depth;
    });
    PIC->registerAfterPassCallback([](StringRef PassID,Any IR, const PreservedAnalyses &PA){
      --Depth;
    });
  llvm::errs()<<"Tool Loaded, PassBuilder Attached\n";
  }};
}
