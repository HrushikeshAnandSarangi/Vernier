#ifndef VERNIER_TOOL_COMPARE_H
#define VERNIER_TOOL_COMPARE_H

#include <string>

namespace vernier {

int runCompare(const std::string &JsonPathA, const std::string &JsonPathB,
               const std::string &OutPath = "");

} // namespace vernier

#endif // VERNIER_TOOL_COMPARE_H
