#ifndef VERNIER_DIFF_H
#define VERNIER_DIFF_H

#include "llvm/ADT/StringRef.h"
#include <cstddef>
#include <string>

namespace vernier {

struct DiffOptions {
  size_t ContextLines = 3;
  size_t MaxDiffLines = 500;
};

std::string computeUnifiedDiff(llvm::StringRef Name,
                               llvm::StringRef BeforeText,
                               llvm::StringRef AfterText,
                               const DiffOptions &Opts = {});

} // namespace vernier

#endif // VERNIER_DIFF_H
