cmake_minimum_required(VERSION 2.8)
project (cloog)

set(CMAKE_CXX_STANDARD 11)

# build shared option (shared not supported in windows yet)
option(CLOOG_BUILD_SHARED "Build shared library" OFF)

if(CLOOG_BUILD_SHARED)
    add_library(cloog SHARED cloog.cpp cloog.h)
    target_compile_definitions(cloog PUBLIC CLOOG_SHARED_LIB)
else()
    add_library(cloog STATIC cloog.cpp cloog.h)
endif()
