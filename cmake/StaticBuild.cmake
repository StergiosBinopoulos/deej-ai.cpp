set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(OpenMP COMPONENTS CXX)

set(ONNX_STATIC_DIR ${CMAKE_SOURCE_DIR}/onnxruntime-build/onnxruntime)
add_executable(deej-ai 
    src/main.cpp
    ${DEEJAI_SOURCES}
)

target_include_directories(deej-ai PRIVATE
    ${DEEJAI_INCLUDES}
    ${CMAKE_SOURCE_DIR}/onnxruntime-build/output/static_lib/Release/include
)

target_link_libraries(deej-ai PRIVATE
    Eigen3::Eigen
    OpenMP::OpenMP_CXX
    ${CMAKE_SOURCE_DIR}/onnxruntime-build/output/static_lib/Release/lib/onnxruntime.lib
)

set_target_properties(deej-ai PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${BIN_DIR}
)
