#ifndef VERNIER_JSON_H
#define VERNIER_JSON_H

#include "Vernier/Schema.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace vernier {

void writeSessionJSON(const schema::SessionRun &Run, llvm::raw_ostream &OS);
bool emitSessionJSONFile(const schema::SessionRun &Run, const std::string &FilePath);

} // namespace vernier

#endif // VERNIER_JSON_H
