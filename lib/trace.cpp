#include "Vernier/Trace.h"
#include "Vernier/Session.h"
#include "Vernier/support.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Process.h"
#include <cstdlib>

using namespace llvm;

namespace vernier {

static cl::OptionCategory VernierCategory("Vernier Options");

cl::opt<std::string> VnOut("vn-out",
                           cl::desc("Output file path for Vernier JSON"),
                           cl::value_desc("filename"),
                           cl::cat(VernierCategory));

cl::opt<bool> VnDiff("vn-diff",
                     cl::desc("Emit unified IR diffs for mutating passes"),
                     cl::init(false), cl::cat(VernierCategory));

cl::opt<bool> VnRemarks("vn-remarks",
                        cl::desc("Capture missed-optimization remarks"),
                        cl::init(false), cl::cat(VernierCategory));

cl::opt<std::string>
    VnDisablePass("vn-disable-pass",
                  cl::desc("Disable specific pass for ablation"),
                  cl::value_desc("pass_id"), cl::cat(VernierCategory));

void registerVernierCallbacks(PassInstrumentationCallbacks &PIC) {
  if (!VnOut.empty()) {
    Session::getInstance().setOutputPath(VnOut);
  }

  PIC.registerShouldRunOptionalPassCallback([](StringRef PassID, Any /*IR*/) {
    // Check command line flag
    if (!VnDisablePass.empty() && PassID == VnDisablePass)
      return false;

    // Check environment variable
    if (const char *EnvDisable = std::getenv("VERNIER_DISABLE_PASS")) {
      if (PassID == EnvDisable)
        return false;
    }

    return true;
  });

  PIC.registerBeforeNonSkippedPassCallback([](StringRef PassID, Any IR) {
    IRUnit U = describe(IR);
    Session::getInstance().enterPass(PassID, U);
  });

  PIC.registerBeforeSkippedPassCallback([](StringRef PassID, Any IR) {
    IRUnit U = describe(IR);
    Session::getInstance().recordSkippedPass(PassID, U);
  });

  PIC.registerAfterPassCallback(
      [](StringRef PassID, Any IR, const PreservedAnalyses &PA) {
        IRUnit U = describe(IR);
        Session::getInstance().exitPass(PassID, U, PA.areAllPreserved());
      });

  PIC.registerAfterPassInvalidatedCallback(
      [](StringRef PassID, const PreservedAnalyses &PA) {
        Session::getInstance().recordInvalidatedPass(PassID, PA.areAllPreserved());
      });
}

} // namespace vernier
