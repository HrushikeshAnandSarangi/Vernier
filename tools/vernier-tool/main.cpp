#include "ablate.h"
#include "compare.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::OptionCategory ToolCategory("Vernier Tool Options");

static cl::opt<std::string>
    SubCommand(cl::Positional, cl::desc("<subcommand: ablate | compare | report>"),
               cl::Required, cl::cat(ToolCategory));

static cl::list<std::string>
    PositionalFiles(cl::Positional, cl::ZeroOrMore, cl::desc("[files...]"),
                    cl::cat(ToolCategory));

static cl::opt<std::string> JsonFile("json", cl::desc("Path to Vernier JSON file"),
                                     cl::value_desc("file.vernier.json"),
                                     cl::cat(ToolCategory));

static cl::opt<std::string> JsonFileA("json-a", cl::desc("Path to first Vernier JSON file (for compare)"),
                                      cl::value_desc("a.vernier.json"),
                                      cl::cat(ToolCategory));

static cl::opt<std::string> JsonFileB("json-b", cl::desc("Path to second Vernier JSON file (for compare)"),
                                      cl::value_desc("b.vernier.json"),
                                      cl::cat(ToolCategory));

static cl::opt<std::string> OutFile("out", cl::desc("Output file path"),
                                    cl::value_desc("output.txt"),
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
    std::string TargetJson = JsonFile;
    if (TargetJson.empty() && !PositionalFiles.empty())
      TargetJson = PositionalFiles[0];
    if (TargetJson.empty()) {
      errs() << "Error: 'ablate' requires --json <file.vernier.json>\n";
      return 1;
    }
    return vernier::runAblate(TargetJson, TopN, CmdOverride);
  }

  if (SubCommand == "compare") {
    std::string FileA = JsonFileA;
    std::string FileB = JsonFileB;
    if (FileA.empty() && PositionalFiles.size() >= 1)
      FileA = PositionalFiles[0];
    if (FileB.empty() && PositionalFiles.size() >= 2)
      FileB = PositionalFiles[1];

    if (FileA.empty() || FileB.empty()) {
      errs() << "Error: 'compare' requires two JSON files.\n"
             << "Usage: vernier-tool compare <runA.vernier.json> <runB.vernier.json> [--out report.txt]\n"
             << "   or: vernier-tool compare --json-a <A> --json-b <B> [--out report.txt]\n";
      return 1;
    }
    return vernier::runCompare(FileA, FileB, OutFile);
  }

  if (SubCommand == "report") {
    errs() << "Vernier: 'report' subcommand will run in Phase 8\n";
    return 0;
  }

  errs() << "Unknown subcommand: " << SubCommand << "\n";
  errs() << "Available subcommands: ablate, compare, report\n";
  return 1;
}
