#ifndef VERNIER_STORE_H
#define VERNIER_STORE_H

#include <map>
#include <mutex>
#include <string>

namespace llvm {
class Function;
class Module;
} // namespace llvm

namespace vernier {

class ContentAddressedStore {
public:
  explicit ContentAddressedStore(const std::string &StoreDir);

  bool ensureDirectories();

  std::string storeFunctionBody(const llvm::Function &F);
  std::map<std::string, std::string> captureSnapshot(const llvm::Module &M);

  void writeManifest();

  const std::string &getStoreDir() const { return StoreDir; }
  uint64_t getTotalObjectsStored() const { return ObjectsStored; }
  uint64_t getTotalBytesSaved() const { return BytesSaved; }

private:
  std::string StoreDir;
  std::string ObjectsDir;
  std::mutex StoreMutex;
  uint64_t ObjectsStored = 0;
  uint64_t ObjectsDeduplicated = 0;
  uint64_t BytesSaved = 0;
  bool Initialized = false;
};

} // namespace vernier

#endif // VERNIER_STORE_H
