cmake_minimum_required(VERSION 3.14)
project(Deej-AI.cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O3 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer -fcolor-diagnostics")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

set(BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(LIB_DIR "${CMAKE_BINARY_DIR}/lib")
set(DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")

file(MAKE_DIRECTORY ${LIB_DIR})
file(MAKE_DIRECTORY ${DEPS_DIR})

execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)

# Get the dependencies
include(cmake/FetchEigen.cmake)
include(cmake/FetchONNX.cmake)
include(cmake/DeejAISources.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libswresample libavutil)

add_executable(deej-ai 
  src/main.cpp
  ${DEEJAI_SOURCES}
)

target_include_directories(deej-ai PRIVATE
  ${DEEJAI_INCLUDES}
)

set_target_properties(deej-ai PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/../lib/onnxruntime/lib"
    SKIP_BUILD_RPATH FALSE
)

target_link_libraries(deej-ai PRIVATE
    PkgConfig::FFMPEG
    Eigen3::Eigen
    onnxruntime
)

include(cmake/Package.cmake)
