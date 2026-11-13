#ifndef VERNIER_TRACE_H
#define VERNIER_TRACE_H

#include "llvm/IR/PassInstrumentation.h"
#include "llvm/Support/CommandLine.h"

namespace vernier {

extern llvm::cl::opt<std::string> VnOut;
extern llvm::cl::opt<bool> VnDiff;
extern llvm::cl::opt<bool> VnRemarks;
extern llvm::cl::opt<std::string> VnDisablePass;

void registerVernierCallbacks(llvm::PassInstrumentationCallbacks &PIC);

} // namespace vernier

#endif // VERNIER_TRACE_H
