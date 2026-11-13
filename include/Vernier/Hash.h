#ifndef VERNIER_HASH_H
#define VERNIER_HASH_H

#include "Vernier/support.h"
#include <cstdint>
#include <string>

namespace vernier {

uint64_t computeIRHashValue(const IRUnit &Unit);
std::string computeIRHash(const IRUnit &Unit);
std::string getIRText(const IRUnit &Unit);

} // namespace vernier

#endif // VERNIER_HASH_H
