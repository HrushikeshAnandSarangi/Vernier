#include "Vernier/Hash.h"
#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/StructuralHash.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace vernier {

uint64_t computeIRHashValue(const IRUnit &Unit) {
  if (Unit.F) {
    return StructuralHash(*Unit.F, true);
  }
  if (Unit.M) {
    return StructuralHash(*Unit.M, true);
  }
  if (Unit.L && Unit.L->getHeader()) {
    if (const Function *F = Unit.L->getHeader()->getParent())
      return StructuralHash(*F, true);
  }
  return 0;
}

std::string computeIRHash(const IRUnit &Unit) {
  uint64_t Val = computeIRHashValue(Unit);
  return formatv("{0:x-16}", Val).str();
}

std::string getIRText(const IRUnit &Unit) {
  std::string Buffer;
  raw_string_ostream OS(Buffer);

  if (Unit.F) {
    Unit.F->print(OS);
  } else if (Unit.M) {
    Unit.M->print(OS, nullptr);
  } else if (Unit.L) {
    Unit.L->print(OS);
  }

  return Buffer;
}

} // namespace vernier
