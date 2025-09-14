set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O3 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer -fcolor-diagnostics")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)

include(cmake/FetchONNX.cmake)

add_executable(deej-ai 
  src/main.cpp
  ${DEEJAI_SOURCES}
)

target_include_directories(deej-ai PRIVATE
  ${DEEJAI_INCLUDES}
)

if(APPLE)
    set(BIN_ORIGIN "@loader_path")
else()
    set(BIN_ORIGIN "$ORIGIN")
endif()

set_target_properties(deej-ai PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH ${BIN_ORIGIN}/../lib/onnxruntime/lib
    SKIP_BUILD_RPATH FALSE
)

target_link_libraries(deej-ai PRIVATE
    Eigen3::Eigen
    onnxruntime
)
