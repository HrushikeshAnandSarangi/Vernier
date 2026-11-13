#ifndef VERNIER_TOOL_ABLATE_H
#define VERNIER_TOOL_ABLATE_H

#include <cstddef>
#include <string>

namespace vernier {

int runAblate(const std::string &JsonPath, size_t TopN,
              const std::string &CmdOverride);

} // namespace vernier

#endif // VERNIER_TOOL_ABLATE_H
