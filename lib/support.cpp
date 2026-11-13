#include "Vernier/support.h"
#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace vernier {

StringRef KindName(IRUnit::Kind K) {
  switch (K) {
  case IRUnit::Kind::Module:
    return "module";
  case IRUnit::Kind::Function:
    return "function";
  case IRUnit::Kind::SCC:
    return "scc";
  case IRUnit::Kind::Loop:
    return "loop";
  case IRUnit::Kind::Unknown:
    break;
  }
  return "unknown";
}

IRUnit describe(Any IR) {
  IRUnit U;

  if (const Module *const *P = any_cast<const Module *>(&IR)) {
    U.K = IRUnit::Kind::Module;
    U.M = *P;
    U.Name = (*P)->getName().str();
    return U;
  }
  if (Module *const *P = any_cast<Module *>(&IR)) {
    U.K = IRUnit::Kind::Module;
    U.M = *P;
    U.Name = (*P)->getName().str();
    return U;
  }

  if (const Function *const *P = any_cast<const Function *>(&IR)) {
    U.K = IRUnit::Kind::Function;
    U.F = *P;
    U.M = (*P)->getParent();
    U.Name = (*P)->getName().str();
    return U;
  }
  if (Function *const *P = any_cast<Function *>(&IR)) {
    U.K = IRUnit::Kind::Function;
    U.F = *P;
    U.M = (*P)->getParent();
    U.Name = (*P)->getName().str();
    return U;
  }

  if (const Loop *const *P = any_cast<const Loop *>(&IR)) {
    U.K = IRUnit::Kind::Loop;
    U.L = *P;
    U.Name = (*P)->getName().str();
    if (const auto *Header = (*P)->getHeader()) {
      U.F = Header->getParent();
      if (U.F)
        U.M = U.F->getParent();
    }
    return U;
  }
  if (Loop *const *P = any_cast<Loop *>(&IR)) {
    U.K = IRUnit::Kind::Loop;
    U.L = *P;
    U.Name = (*P)->getName().str();
    if (const auto *Header = (*P)->getHeader()) {
      U.F = Header->getParent();
      if (U.F)
        U.M = U.F->getParent();
    }
    return U;
  }

  if (const LazyCallGraph::SCC *const *P = any_cast<const LazyCallGraph::SCC *>(&IR)) {
    U.K = IRUnit::Kind::SCC;
    U.Name = (*P)->getName();
    return U;
  }
  if (LazyCallGraph::SCC *const *P = any_cast<LazyCallGraph::SCC *>(&IR)) {
    U.K = IRUnit::Kind::SCC;
    U.Name = (*P)->getName();
    return U;
  }

  if (const LazyCallGraph::Node *const *P = any_cast<const LazyCallGraph::Node *>(&IR)) {
    U.K = IRUnit::Kind::Function;
    U.F = &(*P)->getFunction();
    U.M = U.F->getParent();
    U.Name = U.F->getName().str();
    return U;
  }
  if (LazyCallGraph::Node *const *P = any_cast<LazyCallGraph::Node *>(&IR)) {
    U.K = IRUnit::Kind::Function;
    U.F = &(*P)->getFunction();
    U.M = U.F->getParent();
    U.Name = U.F->getName().str();
    return U;
  }

  U.Name = "<unknown>";
  return U;
}

} // namespace vernier
