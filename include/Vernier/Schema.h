#ifndef VERNIER_SCHEMA_H
#define VERNIER_SCHEMA_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vernier {
namespace schema {

struct RunMetadata {
  std::string Version = "vernier/v1";
  std::string CompilerVersion;
  std::vector<std::string> CommandLine;
  std::string InputFile;
  std::string OutputFile;
  std::string TimestampISO;
  std::string StorePath;
};

struct RunSummary {
  uint64_t TotalWallNs = 0;
  uint64_t TotalInvocations = 0;
  uint64_t MutatingInvocations = 0;
  uint64_t SkippedInvocations = 0;
  uint64_t TotalRemarks = 0;
};

struct UnitInfo {
  std::string Kind; // "module", "function", "loop", "scc", "unknown"
  std::string Name;
  std::string ParentModule;
};

struct TimingInfo {
  uint64_t StartNs = 0;
  uint64_t EndNs = 0;
  uint64_t SelfNs = 0;
  uint64_t TotalNs = 0;
};

struct ChangeInfo {
  bool Changed = false;
  bool AllPreserved = false;
  bool UnderReporting = false;
  std::string BeforeHash;
  std::string AfterHash;
};

struct StructuralSignificance {
  int64_t DeltaInstructions = 0;
  int64_t DeltaBasicBlocks = 0;
  int64_t DeltaCFGEdges = 0;
  int64_t DeltaLoops = 0;
};

struct MemorySignificance {
  int64_t DeltaLoads = 0;
  int64_t DeltaStores = 0;
  int64_t DeltaAllocas = 0;
};

struct CallGraphSignificance {
  int64_t DeltaDirectCalls = 0;
  int64_t DeltaIndirectCalls = 0;
  int64_t InlinedFunctions = 0;
};

struct CostSignificance {
  double TTICostDelta = 0.0;
  double WeightedHotpathCostDelta = 0.0;
};

struct SignificanceInfo {
  StructuralSignificance Structural;
  MemorySignificance Memory;
  CallGraphSignificance CallGraph;
  uint32_t MaxVectorWidth = 1;
  CostSignificance Cost;
};

struct InvocationRecord {
  uint64_t Id = 0;
  std::string PassID;
  UnitInfo Unit;
  int Depth = 0;
  bool Skipped = false;
  bool Invalidated = false;
  TimingInfo Timing;
  ChangeInfo Change;
  SignificanceInfo Significance;
  std::string DiffId;
  std::vector<std::string> RemarkIds;
};

struct RemarkRecord {
  std::string Id;
  uint64_t InvocationId = 0;
  std::string PassName;
  std::string Function;
  std::string File;
  uint32_t Line = 0;
  uint32_t Column = 0;
  std::string Message;
};

struct BaselineAblation {
  uint64_t ObjectSizeBytes = 0;
  uint64_t MachineInstructions = 0;
};

struct CandidateAblation {
  std::string PassID;
  uint64_t AblatedSizeBytes = 0;
  uint64_t AblatedMachineInstructions = 0;
  int64_t DeltaSizeBytes = 0;
  int64_t DeltaMachineInstructions = 0;
  std::string Status; // "success", "skipped_required", "failed"
};

struct AblationData {
  BaselineAblation Baseline;
  std::vector<CandidateAblation> Candidates;
};

struct SessionRun {
  std::string Schema = "vernier/v1";
  RunMetadata Metadata;
  RunSummary Summary;
  std::vector<InvocationRecord> Invocations;
  std::map<std::string, std::string> Diffs;
  std::vector<RemarkRecord> Remarks;
  std::map<std::string, std::map<std::string, std::string>> Snapshots;
  AblationData Ablation;
};

} // namespace schema
} // namespace vernier

#endif // VERNIER_SCHEMA_H
