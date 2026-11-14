cmake_minimum_required(VERSION 3.20)
project(CustomTransform CXX)

add_library(custom_transform MODULE
  CustomTransform.cpp
)

target_include_directories(custom_transform SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(custom_transform PRIVATE ${LLVM_DEFS})

if(TARGET LLVM)
  target_link_libraries(custom_transform PRIVATE LLVM)
elseif(LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(custom_transform PRIVATE LLVM)
endif()

target_compile_options(custom_transform PRIVATE
  -Wall
  -Wextra
)

if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(custom_transform PRIVATE -fno-rtti)
endif()

if(NOT LLVM_ENABLE_EH)
  target_compile_options(custom_transform PRIVATE -fno-exceptions)
endif()

set_target_properties(custom_transform PROPERTIES PREFIX "")
