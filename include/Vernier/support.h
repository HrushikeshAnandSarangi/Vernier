#ifndef VERNIER_SUPPORT_H
#define VERNIER_SUPPORT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/PassInstrumentation.h"
#include "string"

namespace vernier{
struct IRUnit{
  enum class Kind{Module, Function, SCC, Loop,Unknown};
  Kind K=Kind::Unknown;
std::string Name;
};
IRUnit describe(llvm::Any IR);
llvm::StringRef KindName(IRUnit::Kind K);
}

#endif
