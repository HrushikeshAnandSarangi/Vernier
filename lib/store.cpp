#include "Vernier/Store.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/StructuralHash.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace vernier {

ContentAddressedStore::ContentAddressedStore(const std::string &StoreDir)
    : StoreDir(StoreDir) {
  SmallString<128> ObjPath(StoreDir);
  sys::path::append(ObjPath, "objects");
  ObjectsDir = std::string(ObjPath.str());
}

bool ContentAddressedStore::ensureDirectories() {
  std::lock_guard<std::mutex> Lock(StoreMutex);
  if (Initialized)
    return true;

  std::error_code EC = sys::fs::create_directories(ObjectsDir);
  if (EC) {
    errs() << "Vernier: Failed to create store directory '" << ObjectsDir
           << "': " << EC.message() << "\n";
    return false;
  }
  Initialized = true;
  return true;
}

std::string ContentAddressedStore::storeFunctionBody(const Function &F) {
  if (!ensureDirectories())
    return "";

  uint64_t HashVal = StructuralHash(F, true);
  std::string Hash = formatv("{0:x-16}", HashVal).str();

  SmallString<128> FilePath(ObjectsDir);
  sys::path::append(FilePath, Hash + ".ll");

  std::lock_guard<std::mutex> Lock(StoreMutex);
  if (sys::fs::exists(FilePath)) {
    ++ObjectsDeduplicated;
    return Hash;
  }

  std::error_code EC;
  raw_fd_ostream OS(FilePath, EC, sys::fs::OF_None);
  if (EC) {
    errs() << "Vernier: Failed to write object '" << FilePath
           << "': " << EC.message() << "\n";
    return Hash;
  }

  F.print(OS);
  OS.flush();
  ++ObjectsStored;

  return Hash;
}

std::map<std::string, std::string>
ContentAddressedStore::captureSnapshot(const Module &M) {
  std::map<std::string, std::string> Snapshot;
  for (const Function &F : M) {
    if (!F.isDeclaration()) {
      Snapshot[F.getName().str()] = storeFunctionBody(F);
    }
  }
  return Snapshot;
}

void ContentAddressedStore::writeManifest() {
  if (!ensureDirectories())
    return;

  SmallString<128> ManifestPath(StoreDir);
  sys::path::append(ManifestPath, "manifest.json");

  std::error_code EC;
  raw_fd_ostream OS(ManifestPath, EC, sys::fs::OF_None);
  if (EC)
    return;

  OS << "{\n"
     << "  \"store_version\": \"1.0.0\",\n"
     << "  \"objects_stored\": " << ObjectsStored << ",\n"
     << "  \"objects_deduplicated\": " << ObjectsDeduplicated << "\n"
     << "}\n";
  OS.flush();
}

} // namespace vernier
