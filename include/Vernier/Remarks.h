#ifndef VERNIER_REMARKS_H
#define VERNIER_REMARKS_H

#include "llvm/IR/DiagnosticHandler.h"
#include <memory>

namespace llvm {
class LLVMContext;
} // namespace llvm

namespace vernier {

class VernierDiagnosticHandler : public llvm::DiagnosticHandler {
public:
  explicit VernierDiagnosticHandler(
      std::unique_ptr<llvm::DiagnosticHandler> OldHandler = nullptr);

  bool handleDiagnostics(const llvm::DiagnosticInfo &DI) override;

  bool isMissedOptRemarkEnabled(llvm::StringRef PassName) const override;
  bool isPassedOptRemarkEnabled(llvm::StringRef PassName) const override;
  bool isAnalysisRemarkEnabled(llvm::StringRef PassName) const override;
  bool isAnyRemarkEnabled() const override;

private:
  std::unique_ptr<llvm::DiagnosticHandler> OldHandler;
};

void installDiagnosticHandler(llvm::LLVMContext &Ctx);

} // namespace vernier

#endif // VERNIER_REMARKS_H
