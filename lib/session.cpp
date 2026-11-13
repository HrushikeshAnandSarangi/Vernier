#include "Vernier/Session.h"
#include "Vernier/Diff.h"
#include "Vernier/Hash.h"
#include "Vernier/JSON.h"
#include "Vernier/Store.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace llvm;

namespace vernier {

static thread_local std::vector<PassFrame> ThreadFrameStack;

Session &Session::getInstance() {
  static Session Instance;
  return Instance;
}

Session::Session() {
  GlobalStartTime = std::chrono::steady_clock::now();
  inferMetadata();
  std::atexit([]() {
    Session::getInstance().flush();
  });
}

Session::~Session() {
  flush();
}

void Session::inferMetadata() {
  Run.Metadata.Version = "vernier/v1";
  Run.Metadata.CompilerVersion = "LLVM " LLVM_VERSION_STRING;

  // Capture /proc/self/cmdline on Linux/WSL
  std::ifstream CmdlineFile("/proc/self/cmdline", std::ios::binary);
  if (CmdlineFile.is_open()) {
    std::string Token;
    while (std::getline(CmdlineFile, Token, '\0')) {
      if (!Token.empty())
        Run.Metadata.CommandLine.push_back(Token);
    }
  }

  // Format current ISO 8601 UTC timestamp
  auto NowTime = std::chrono::system_clock::now();
  std::time_t NowC = std::chrono::system_clock::to_time_t(NowTime);
  std::tm TmBuffer{};
#if defined(_WIN32)
  gmtime_s(&TmBuffer, &NowC);
#else
  gmtime_r(&NowC, &TmBuffer);
#endif
  char IsoBuf[64];
  if (std::strftime(IsoBuf, sizeof(IsoBuf), "%Y-%m-%dT%H:%M:%SZ", &TmBuffer))
    Run.Metadata.TimestampISO = IsoBuf;
}

void Session::ensureStoreInitialized() {
  if (CASStore)
    return;

  std::string OutPath = getOutputPath();
  SmallString<128> StoreDir(OutPath);
  if (StringRef(StoreDir).ends_with(".vernier.json")) {
    StoreDir.resize(StoreDir.size() - StringRef(".vernier.json").size());
    StoreDir.append(".vernier.store");
  } else {
    sys::path::replace_extension(StoreDir, "vernier.store");
  }
  CASStore = std::make_unique<ContentAddressedStore>(std::string(StoreDir.str()));
  Run.Metadata.StorePath = std::string(sys::path::filename(StoreDir));
}

void Session::initialize(const std::string &InputFile,
                         const std::string &OutputFile) {
  std::lock_guard<std::mutex> Lock(SinkMutex);
  if (!InputFile.empty())
    ResolvedInputFile = InputFile;
  if (!OutputFile.empty())
    ResolvedOutputFile = OutputFile;
  Run.Metadata.InputFile = ResolvedInputFile;
  Run.Metadata.OutputFile = ResolvedOutputFile;
  ensureStoreInitialized();
}

void Session::setOutputPath(const std::string &Path) {
  std::lock_guard<std::mutex> Lock(SinkMutex);
  CustomOutputPath = Path;
  if (CASStore) {
    CASStore.reset();
  }
  ensureStoreInitialized();
}

std::string Session::getOutputPath() const {
  if (!CustomOutputPath.empty())
    return CustomOutputPath;

  if (!ResolvedInputFile.empty()) {
    SmallString<128> Path(ResolvedInputFile);
    sys::path::replace_extension(Path, "vernier.json");
    return std::string(Path.str());
  }

  return "vernier_run.json";
}

void Session::setEmitDiffs(bool Enable) {
  EmitDiffs.store(Enable);
}

bool Session::getEmitDiffs() const {
  return EmitDiffs.load();
}

ContentAddressedStore *Session::getStore() {
  std::lock_guard<std::mutex> Lock(SinkMutex);
  return CASStore.get();
}

void Session::enterPass(StringRef PassID, const IRUnit &Unit) {
  if (ResolvedInputFile.empty() && Unit.M) {
    std::string SourceFile = Unit.M->getSourceFileName();
    if (!SourceFile.empty() && SourceFile != "empty.ll")
      initialize(SourceFile, ResolvedOutputFile);
    else if (!Unit.M->getName().empty())
      initialize(Unit.M->getName().str(), ResolvedOutputFile);
  }

  auto Now = std::chrono::steady_clock::now();
  uint64_t StartNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         Now - GlobalStartTime)
                         .count();
  int Depth = static_cast<int>(ThreadFrameStack.size());

  std::string BeforeHash = computeIRHash(Unit);
  std::string BeforeText;
  if (EmitDiffs.load()) {
    BeforeText = getIRText(Unit);
  }

  PassFrame Frame;
  Frame.InvocationId = NextInvocationId.fetch_add(1);
  Frame.PassID = PassID.str();
  Frame.Unit = Unit;
  Frame.Depth = Depth;
  Frame.StartPoint = Now;
  Frame.StartNs = StartNs;
  Frame.ChildDurationNs = 0;
  Frame.BeforeHash = std::move(BeforeHash);
  Frame.BeforeText = std::move(BeforeText);

  ThreadFrameStack.push_back(std::move(Frame));
}

void Session::exitPass(StringRef /*PassID*/, const IRUnit &Unit,
                       bool AllPreserved) {
  if (ThreadFrameStack.empty())
    return;

  PassFrame Frame = std::move(ThreadFrameStack.back());
  ThreadFrameStack.pop_back();

  auto Now = std::chrono::steady_clock::now();
  uint64_t EndNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       Now - GlobalStartTime)
                       .count();
  uint64_t TotalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         Now - Frame.StartPoint)
                         .count();
  uint64_t SelfNs = (TotalNs >= Frame.ChildDurationNs)
                        ? (TotalNs - Frame.ChildDurationNs)
                        : 0;

  if (!ThreadFrameStack.empty())
    ThreadFrameStack.back().ChildDurationNs += TotalNs;

  std::string AfterHash = computeIRHash(Unit);
  bool Changed = (!Frame.BeforeHash.empty() && !AfterHash.empty() &&
                  Frame.BeforeHash != AfterHash);

  std::string DiffId;
  if (Changed && EmitDiffs.load()) {
    std::string AfterText = getIRText(Unit);
    std::string Diff = computeUnifiedDiff(Unit.Name, Frame.BeforeText, AfterText);
    if (!Diff.empty()) {
      DiffId = formatv("diff_{0}", Frame.InvocationId).str();
      std::lock_guard<std::mutex> Lock(SinkMutex);
      Run.Diffs[DiffId] = std::move(Diff);
    }
  }

  // Content-addressed IR snapshot recording
  if (Changed) {
    std::lock_guard<std::mutex> Lock(SinkMutex);
    ensureStoreInitialized();
    if (CASStore) {
      std::string SnapKey = formatv("inv_{0}_after", Frame.InvocationId).str();
      if (Unit.F) {
        std::string BodyHash = CASStore->storeFunctionBody(*Unit.F);
        Run.Snapshots[SnapKey][Unit.F->getName().str()] = BodyHash;
      } else if (Unit.M) {
        Run.Snapshots[SnapKey] = CASStore->captureSnapshot(*Unit.M);
      }
    }
  }

  schema::InvocationRecord Rec;
  Rec.Id = Frame.InvocationId;
  Rec.PassID = Frame.PassID;
  Rec.Unit.Kind = KindName(Frame.Unit.K).str();
  Rec.Unit.Name = Frame.Unit.Name;
  Rec.Unit.ParentModule = Frame.Unit.M ? Frame.Unit.M->getName().str() : "";
  Rec.Depth = Frame.Depth;
  Rec.Skipped = false;
  Rec.Invalidated = false;

  Rec.Timing.StartNs = Frame.StartNs;
  Rec.Timing.EndNs = EndNs;
  Rec.Timing.SelfNs = SelfNs;
  Rec.Timing.TotalNs = TotalNs;

  Rec.Change.Changed = Changed;
  Rec.Change.AllPreserved = AllPreserved;
  Rec.Change.UnderReporting = (!AllPreserved && !Changed);
  Rec.Change.BeforeHash = Frame.BeforeHash;
  Rec.Change.AfterHash = AfterHash;
  Rec.DiffId = DiffId;

  {
    std::lock_guard<std::mutex> Lock(SinkMutex);
    Run.Invocations.push_back(std::move(Rec));
    if (Changed)
      ++Run.Summary.MutatingInvocations;
  }
}

void Session::recordSkippedPass(StringRef PassID, const IRUnit &Unit) {
  auto Now = std::chrono::steady_clock::now();
  uint64_t NowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       Now - GlobalStartTime)
                       .count();
  int Depth = static_cast<int>(ThreadFrameStack.size());

  schema::InvocationRecord Rec;
  Rec.Id = NextInvocationId.fetch_add(1);
  Rec.PassID = PassID.str();
  Rec.Unit.Kind = KindName(Unit.K).str();
  Rec.Unit.Name = Unit.Name;
  Rec.Unit.ParentModule = Unit.M ? Unit.M->getName().str() : "";
  Rec.Depth = Depth;
  Rec.Skipped = true;
  Rec.Invalidated = false;

  Rec.Timing.StartNs = NowNs;
  Rec.Timing.EndNs = NowNs;
  Rec.Timing.SelfNs = 0;
  Rec.Timing.TotalNs = 0;

  Rec.Change.Changed = false;
  Rec.Change.AllPreserved = true;
  Rec.Change.UnderReporting = false;

  {
    std::lock_guard<std::mutex> Lock(SinkMutex);
    Run.Invocations.push_back(std::move(Rec));
    ++Run.Summary.SkippedInvocations;
  }
}

void Session::recordInvalidatedPass(StringRef /*PassID*/, bool AllPreserved) {
  if (ThreadFrameStack.empty())
    return;

  PassFrame Frame = std::move(ThreadFrameStack.back());
  ThreadFrameStack.pop_back();

  auto Now = std::chrono::steady_clock::now();
  uint64_t EndNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       Now - GlobalStartTime)
                       .count();
  uint64_t TotalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         Now - Frame.StartPoint)
                         .count();
  uint64_t SelfNs = (TotalNs >= Frame.ChildDurationNs)
                        ? (TotalNs - Frame.ChildDurationNs)
                        : 0;

  if (!ThreadFrameStack.empty())
    ThreadFrameStack.back().ChildDurationNs += TotalNs;

  schema::InvocationRecord Rec;
  Rec.Id = Frame.InvocationId;
  Rec.PassID = Frame.PassID;
  Rec.Unit.Kind = KindName(Frame.Unit.K).str();
  Rec.Unit.Name = Frame.Unit.Name;
  Rec.Unit.ParentModule = Frame.Unit.M ? Frame.Unit.M->getName().str() : "";
  Rec.Depth = Frame.Depth;
  Rec.Skipped = false;
  Rec.Invalidated = true;

  Rec.Timing.StartNs = Frame.StartNs;
  Rec.Timing.EndNs = EndNs;
  Rec.Timing.SelfNs = SelfNs;
  Rec.Timing.TotalNs = TotalNs;

  Rec.Change.Changed = true;
  Rec.Change.AllPreserved = AllPreserved;
  Rec.Change.UnderReporting = false;

  {
    std::lock_guard<std::mutex> Lock(SinkMutex);
    Run.Invocations.push_back(std::move(Rec));
    ++Run.Summary.MutatingInvocations;
  }
}

void Session::flush() {
  if (HasFlushed.exchange(true))
    return;

  std::lock_guard<std::mutex> Lock(SinkMutex);

  ensureStoreInitialized();
  if (CASStore)
    CASStore->writeManifest();

  auto Now = std::chrono::steady_clock::now();
  Run.Summary.TotalWallNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                Now - GlobalStartTime)
                                .count();
  Run.Summary.TotalInvocations = Run.Invocations.size();
  Run.Metadata.InputFile = ResolvedInputFile;
  Run.Metadata.OutputFile = ResolvedOutputFile;

  std::string OutPath = getOutputPath();
  emitSessionJSONFile(Run, OutPath);
}

schema::SessionRun &Session::getRun() {
  return Run;
}

} // namespace vernier
