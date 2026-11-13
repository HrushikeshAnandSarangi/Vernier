#ifndef VERNIER_SESSION_H
#define VERNIER_SESSION_H

#include "Vernier/Schema.h"
#include "Vernier/Store.h"
#include "Vernier/support.h"
#include "llvm/ADT/StringRef.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace vernier {

struct PassFrame {
  uint64_t InvocationId = 0;
  std::string PassID;
  IRUnit Unit;
  int Depth = 0;
  std::chrono::steady_clock::time_point StartPoint;
  uint64_t StartNs = 0;
  uint64_t ChildDurationNs = 0;
  std::string BeforeHash;
  std::string BeforeText;
};

class Session {
public:
  static Session &getInstance();

  void initialize(const std::string &InputFile = "", const std::string &OutputFile = "");

  void enterPass(llvm::StringRef PassID, const IRUnit &Unit);
  void exitPass(llvm::StringRef PassID, const IRUnit &Unit, bool AllPreserved);
  void recordSkippedPass(llvm::StringRef PassID, const IRUnit &Unit);
  void recordInvalidatedPass(llvm::StringRef PassID, bool AllPreserved);

  void setOutputPath(const std::string &Path);
  std::string getOutputPath() const;

  void setEmitDiffs(bool Enable);
  bool getEmitDiffs() const;

  void flush();

  schema::SessionRun &getRun();
  ContentAddressedStore *getStore();

private:
  Session();
  ~Session();

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  std::chrono::steady_clock::time_point GlobalStartTime;
  std::atomic<uint64_t> NextInvocationId{0};

  std::string CustomOutputPath;
  std::string ResolvedInputFile;
  std::string ResolvedOutputFile;
  std::atomic<bool> EmitDiffs{false};

  std::mutex SinkMutex;
  schema::SessionRun Run;
  std::unique_ptr<ContentAddressedStore> CASStore;
  std::atomic<bool> HasFlushed{false};

  void inferMetadata();
  void ensureStoreInitialized();
};

} // namespace vernier

#endif // VERNIER_SESSION_H
