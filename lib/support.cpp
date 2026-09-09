#include "Vernier/support.h"
#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace vernier{
StringRef KindName(IRUnit::Kind K){
  switch(K){
    //extend it as needed
    case IRUnit::Kind::Module: return "module";
    case IRUnit::Kind::Function: return "function";
    case IRUnit::Kind::SCC: return "scc";
    case IRUnit::Kind::Loop: return "loop";
    case IRUnit::Kind::Unknown: break;
  }
  return "unknown";
  
}

IRUnit describe(Any IR){
  IRUnit U;
  if(const Module *const *P=any_cast <const Module *>(&IR)){
    U.K=IRUnit::Kind::Module;
    U.Name=(*P)->getName().str();

  }else if 
}
}
