#ifndef VERNIER_SUPPORT_H
#define VERNIER_SUPPORT_H

#include "llvm/ADT/Any.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace llvm {
class Module;
class Function;
class Loop;
} // namespace llvm

namespace vernier {

struct IRUnit {
  enum class Kind { Module, Function, SCC, Loop, Unknown };
  Kind K = Kind::Unknown;
  std::string Name;
  const llvm::Module *M = nullptr;
  const llvm::Function *F = nullptr;
  const llvm::Loop *L = nullptr;
};

IRUnit describe(llvm::Any IR);
llvm::StringRef KindName(IRUnit::Kind K);

} // namespace vernier

#endif // VERNIER_SUPPORT_H
