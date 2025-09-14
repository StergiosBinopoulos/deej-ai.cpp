cmake_minimum_required(VERSION 3.15)
project(Deej-AI.cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STATIC_BUILD "Build static libraries" OFF)

set(BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(LIB_DIR "${CMAKE_BINARY_DIR}/lib")
set(DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")

file(MAKE_DIRECTORY ${BIN_DIR})
file(MAKE_DIRECTORY ${LIB_DIR})
file(MAKE_DIRECTORY ${DEPS_DIR})

include(cmake/DeejAISources.cmake)
include(cmake/FetchEigen.cmake)

message(STATUS "STATIC_BUILD = ${STATIC_BUILD}")
if(WIN32 AND STATIC_BUILD)
    include(cmake/StaticBuild.cmake)
else()
    include(cmake/DynamicBuild.cmake)
endif()

include(cmake/Package.cmake)
