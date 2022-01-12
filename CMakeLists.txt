cmake_minimum_required(VERSION 3.21.1)
project(ollvm-deobfuscator)

find_package(LLVM REQUIRED CONFIG)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 14 CACHE STRING "")

#if(NOT LLVM_ENABLE_RTTI)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
#endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
include_directories(./include)

add_library(DBogusFlow SHARED ./lib/DBogusFlow.cpp)
add_library(DFlattening SHARED ./lib/DFlattening.cpp)
