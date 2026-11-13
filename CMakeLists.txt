cmake_minimum_required(VERSION 3.20)
project(Vernier CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM OPTIONAL)

separate_arguments(LLVM_DEFS UNIX_COMMAND "${LLVM_DEFINITIONS}")

llvm_map_components_to_libnames(LLVM_LIBS
  Support
  Core
  Analysis
  Passes
  TransformUtils
  TargetParser
)

# Shared plugin module for opt and clang
add_library(vernier MODULE
  lib/diff.cpp
  lib/hash.cpp
  lib/json.cpp
  lib/plugin.cpp
  lib/remarks.cpp
  lib/session.cpp
  lib/significance.cpp
  lib/store.cpp
  lib/support.cpp
  lib/trace.cpp
)

target_include_directories(vernier SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(vernier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(vernier PRIVATE ${LLVM_DEFS})

# In LLVM pass plugins, linking static LLVM component libraries into a MODULE loaded by opt/clang
# causes duplicate cl::opt registration. If the LLVM shared dylib exists, link it; otherwise symbols
# resolve from the host executable at runtime.
if(TARGET LLVM)
  target_link_libraries(vernier PRIVATE LLVM)
elseif(LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(vernier PRIVATE LLVM)
endif()

target_compile_options(vernier PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(vernier PRIVATE -fno-rtti)
endif()

if(NOT LLVM_ENABLE_EH)
  target_compile_options(vernier PRIVATE -fno-exceptions)
endif()

set_target_properties(vernier PROPERTIES PREFIX "")

# Companion CLI tool
add_executable(vernier-tool
  tools/vernier-tool/main.cpp
  tools/vernier-tool/ablate.cpp
  tools/vernier-tool/compare.cpp
  tools/vernier-tool/report.cpp
)

target_include_directories(vernier-tool SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(vernier-tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include tools/vernier-tool)
target_compile_definitions(vernier-tool PRIVATE ${LLVM_DEFS})
target_link_libraries(vernier-tool PRIVATE ${LLVM_LIBS})

target_compile_options(vernier-tool PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(vernier-tool PRIVATE -fno-rtti)
endif()

if(NOT LLVM_ENABLE_EH)
  target_compile_options(vernier-tool PRIVATE -fno-exceptions)
endif()

enable_testing()

# Unit test: Diffing
add_executable(test_diff
  test/test_diff.cpp
  lib/diff.cpp
)
target_include_directories(test_diff SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(test_diff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(test_diff PRIVATE ${LLVM_DEFS})
target_link_libraries(test_diff PRIVATE ${LLVM_LIBS})
add_test(NAME test_diff COMMAND test_diff)

# Unit test: Compare / Sequence Alignment
add_executable(test_compare
  test/test_compare.cpp
  tools/vernier-tool/compare.cpp
)
target_include_directories(test_compare SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(test_compare PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include tools/vernier-tool)
target_compile_definitions(test_compare PRIVATE ${LLVM_DEFS})
target_link_libraries(test_compare PRIVATE ${LLVM_LIBS})
add_test(NAME test_compare COMMAND test_compare)

# Unit test: Report Generation
add_executable(test_report
  test/test_report.cpp
  tools/vernier-tool/report.cpp
)
target_include_directories(test_report SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(test_report PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include tools/vernier-tool)
target_compile_definitions(test_report PRIVATE ${LLVM_DEFS})
target_link_libraries(test_report PRIVATE ${LLVM_LIBS})
add_test(NAME test_report COMMAND test_report)

# Integration smoke test
find_program(BASH_BIN bash)
if(BASH_BIN)
  add_test(NAME test_smoke
    COMMAND ${BASH_BIN} ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke.sh $<TARGET_FILE:vernier> $<TARGET_FILE:vernier-tool> ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()

