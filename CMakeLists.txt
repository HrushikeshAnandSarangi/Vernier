cmake_minimum_required(VERSION 3.20)
project(Vernier CXX)
#setting parameters
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#declaring the plugin module
add_library(vernier MODULE lib/plugin.cpp)

#compilation targets
target_include_directories(vernier SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
#split the string
separate_arguments(LLVM_DEFS UNIX_COMMAND "${LLVM_DEFINITIONS}")
target_compile_definitions(vernier PRIVATE ${LLVM_DEFS})

#rtti fallback
if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(vernier PRIVATE -fno-rtti)
endif()

#dropping the prefix for build artifacts 
set_target_properties(vernier PROPERTIES PREFIX "")
