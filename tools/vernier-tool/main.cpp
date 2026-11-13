#include "ablate.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::OptionCategory ToolCategory("Vernier Tool Options");

static cl::opt<std::string>
    SubCommand(cl::Positional, cl::desc("<subcommand: ablate | compare | report>"),
               cl::Required, cl::cat(ToolCategory));

static cl::opt<std::string> JsonFile("json", cl::desc("Path to Vernier JSON file"),
                                     cl::value_desc("file.vernier.json"),
                                     cl::cat(ToolCategory));

static cl::opt<size_t> TopN("top", cl::desc("Number of top passes to ablate"),
                            cl::init(5), cl::cat(ToolCategory));

static cl::opt<std::string>
    CmdOverride("cmd", cl::desc("Compiler command override for ablation"),
                cl::value_desc("command"), cl::cat(ToolCategory));

int main(int argc, char **argv) {
  cl::HideUnrelatedOptions(ToolCategory);
  cl::ParseCommandLineOptions(argc, argv, "Vernier companion analysis tool\n");

  if (SubCommand == "ablate") {
    if (JsonFile.empty()) {
      errs() << "Error: 'ablate' requires --json <file.vernier.json>\n";
      return 1;
    }
    return vernier::runAblate(JsonFile, TopN, CmdOverride);
  }

  if (SubCommand == "compare") {
    errs() << "Vernier: 'compare' subcommand will run in Phase 7\n";
    return 0;
  }

  if (SubCommand == "report") {
    errs() << "Vernier: 'report' subcommand will run in Phase 8\n";
    return 0;
  }

  errs() << "Unknown subcommand: " << SubCommand << "\n";
  errs() << "Available subcommands: ablate, compare, report\n";
  return 1;
}
