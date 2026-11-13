#include "Vernier/JSON.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <system_error>

using namespace llvm;

namespace vernier {

static json::Object serializeMetadata(const schema::RunMetadata &M) {
  json::Array Cmd;
  for (const auto &Arg : M.CommandLine)
    Cmd.push_back(Arg);

  return json::Object{
      {"version", M.Version},
      {"compiler_version", M.CompilerVersion},
      {"command_line", std::move(Cmd)},
      {"input_file", M.InputFile},
      {"output_file", M.OutputFile},
      {"timestamp_iso", M.TimestampISO},
      {"store_path", M.StorePath},
  };
}

static json::Object serializeSummary(const schema::RunSummary &S) {
  return json::Object{
      {"total_wall_ns", static_cast<int64_t>(S.TotalWallNs)},
      {"total_invocations", static_cast<int64_t>(S.TotalInvocations)},
      {"mutating_invocations", static_cast<int64_t>(S.MutatingInvocations)},
      {"skipped_invocations", static_cast<int64_t>(S.SkippedInvocations)},
      {"total_remarks", static_cast<int64_t>(S.TotalRemarks)},
  };
}

static json::Object serializeInvocation(const schema::InvocationRecord &Inv) {
  json::Object UnitObj{
      {"kind", Inv.Unit.Kind},
      {"name", Inv.Unit.Name},
      {"parent_module", Inv.Unit.ParentModule},
  };

  json::Object TimingObj{
      {"start_ns", static_cast<int64_t>(Inv.Timing.StartNs)},
      {"end_ns", static_cast<int64_t>(Inv.Timing.EndNs)},
      {"self_ns", static_cast<int64_t>(Inv.Timing.SelfNs)},
      {"total_ns", static_cast<int64_t>(Inv.Timing.TotalNs)},
  };

  json::Object ChangeObj{
      {"changed", Inv.Change.Changed},
      {"all_preserved", Inv.Change.AllPreserved},
      {"under_reporting", Inv.Change.UnderReporting},
      {"before_hash", Inv.Change.BeforeHash},
      {"after_hash", Inv.Change.AfterHash},
  };

  json::Object SigStructural{
      {"delta_instructions", Inv.Significance.Structural.DeltaInstructions},
      {"delta_basic_blocks", Inv.Significance.Structural.DeltaBasicBlocks},
      {"delta_cfg_edges", Inv.Significance.Structural.DeltaCFGEdges},
      {"delta_loops", Inv.Significance.Structural.DeltaLoops},
  };

  json::Object SigMemory{
      {"delta_loads", Inv.Significance.Memory.DeltaLoads},
      {"delta_stores", Inv.Significance.Memory.DeltaStores},
      {"delta_allocas", Inv.Significance.Memory.DeltaAllocas},
  };

  json::Object SigCallGraph{
      {"delta_direct_calls", Inv.Significance.CallGraph.DeltaDirectCalls},
      {"delta_indirect_calls", Inv.Significance.CallGraph.DeltaIndirectCalls},
      {"inlined_functions", Inv.Significance.CallGraph.InlinedFunctions},
  };

  json::Object SigCost{
      {"tti_cost_delta", Inv.Significance.Cost.TTICostDelta},
      {"weighted_hotpath_cost_delta",
       Inv.Significance.Cost.WeightedHotpathCostDelta},
  };

  json::Object SigObj{
      {"structural", std::move(SigStructural)},
      {"memory", std::move(SigMemory)},
      {"callgraph", std::move(SigCallGraph)},
      {"max_vector_width", static_cast<int64_t>(Inv.Significance.MaxVectorWidth)},
      {"cost", std::move(SigCost)},
  };

  json::Array RemarkArr;
  for (const auto &R : Inv.RemarkIds)
    RemarkArr.push_back(R);

  return json::Object{
      {"id", static_cast<int64_t>(Inv.Id)},
      {"pass_id", Inv.PassID},
      {"unit", std::move(UnitObj)},
      {"depth", Inv.Depth},
      {"skipped", Inv.Skipped},
      {"invalidated", Inv.Invalidated},
      {"timing", std::move(TimingObj)},
      {"change", std::move(ChangeObj)},
      {"significance", std::move(SigObj)},
      {"diff_id", Inv.DiffId},
      {"remark_ids", std::move(RemarkArr)},
  };
}

void writeSessionJSON(const schema::SessionRun &Run, raw_ostream &OS) {
  json::Array InvocationsArr;
  for (const auto &Inv : Run.Invocations)
    InvocationsArr.push_back(serializeInvocation(Inv));

  json::Object DiffsObj;
  for (const auto &KV : Run.Diffs)
    DiffsObj[KV.first] = KV.second;

  json::Array RemarksArr;
  for (const auto &R : Run.Remarks) {
    RemarksArr.push_back(json::Object{
        {"id", R.Id},
        {"invocation_id", static_cast<int64_t>(R.InvocationId)},
        {"pass_name", R.PassName},
        {"function", R.Function},
        {"file", R.File},
        {"line", static_cast<int64_t>(R.Line)},
        {"column", static_cast<int64_t>(R.Column)},
        {"message", R.Message},
    });
  }

  json::Object SnapshotsObj;
  for (const auto &SnapKV : Run.Snapshots) {
    json::Object FnMap;
    for (const auto &FnKV : SnapKV.second)
      FnMap[FnKV.first] = FnKV.second;
    SnapshotsObj[SnapKV.first] = std::move(FnMap);
  }

  json::Array AblationCandidates;
  for (const auto &C : Run.Ablation.Candidates) {
    AblationCandidates.push_back(json::Object{
        {"pass_id", C.PassID},
        {"ablated_size_bytes", static_cast<int64_t>(C.AblatedSizeBytes)},
        {"ablated_machine_instructions",
         static_cast<int64_t>(C.AblatedMachineInstructions)},
        {"delta_size_bytes", C.DeltaSizeBytes},
        {"delta_machine_instructions", C.DeltaMachineInstructions},
        {"status", C.Status},
    });
  }

  json::Object AblationObj{
      {"baseline",
       json::Object{
           {"object_size_bytes",
            static_cast<int64_t>(Run.Ablation.Baseline.ObjectSizeBytes)},
           {"machine_instructions",
            static_cast<int64_t>(Run.Ablation.Baseline.MachineInstructions)},
       }},
      {"candidates", std::move(AblationCandidates)},
  };

  json::Object Root{
      {"$schema", Run.Schema},
      {"metadata", serializeMetadata(Run.Metadata)},
      {"summary", serializeSummary(Run.Summary)},
      {"invocations", std::move(InvocationsArr)},
      {"diffs", std::move(DiffsObj)},
      {"remarks", std::move(RemarksArr)},
      {"snapshots", std::move(SnapshotsObj)},
      {"ablation", std::move(AblationObj)},
  };

  OS << formatv("{0:2}\n", json::Value(std::move(Root)));
}

bool emitSessionJSONFile(const schema::SessionRun &Run,
                         const std::string &FilePath) {
  std::error_code EC;
  raw_fd_ostream OS(FilePath, EC, sys::fs::OF_None);
  if (EC) {
    errs() << "Vernier: Failed to open JSON output file '" << FilePath
           << "': " << EC.message() << "\n";
    return false;
  }

  writeSessionJSON(Run, OS);
  OS.flush();
  return true;
}

} // namespace vernier
