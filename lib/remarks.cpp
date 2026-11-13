#include "Vernier/Remarks.h"
#include "Vernier/Session.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/LLVMContext.h"

using namespace llvm;

namespace vernier {

VernierDiagnosticHandler::VernierDiagnosticHandler(
    std::unique_ptr<DiagnosticHandler> OldHandler)
    : OldHandler(std::move(OldHandler)) {}

bool VernierDiagnosticHandler::handleDiagnostics(const DiagnosticInfo &DI) {
  if (const auto *OptRemark = dyn_cast<DiagnosticInfoOptimizationBase>(&DI)) {
    std::string PassName = OptRemark->getPassName().str();
    std::string FuncName = OptRemark->getFunction().getName().str();
    std::string Msg = OptRemark->getMsg();
    std::string File;
    unsigned Line = 0, Col = 0;
    if (OptRemark->getLocation().isValid()) {
      File = OptRemark->getLocation().getRelativePath().str();
      Line = OptRemark->getLocation().getLine();
      Col = OptRemark->getLocation().getColumn();
    }
    Session::getInstance().recordRemark(PassName, FuncName, File, Line, Col, Msg);
    return true;
  }

  if (OldHandler)
    return OldHandler->handleDiagnostics(DI);

  return false;
}

bool VernierDiagnosticHandler::isMissedOptRemarkEnabled(StringRef /*PassName*/) const {
  return true;
}

bool VernierDiagnosticHandler::isPassedOptRemarkEnabled(StringRef /*PassName*/) const {
  return true;
}

bool VernierDiagnosticHandler::isAnalysisRemarkEnabled(StringRef /*PassName*/) const {
  return true;
}

bool VernierDiagnosticHandler::isAnyRemarkEnabled() const {
  return true;
}

void installDiagnosticHandler(LLVMContext &Ctx) {
  auto Old = Ctx.getDiagnosticHandler();
  Ctx.setDiagnosticHandler(
      std::make_unique<VernierDiagnosticHandler>(std::move(Old)));
}

} // namespace vernier
