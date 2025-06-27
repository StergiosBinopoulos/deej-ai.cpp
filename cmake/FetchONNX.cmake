set(ONNX_RUNTIME_VERSION "1.22.0" CACHE STRING "ONNX Runtime version")
set(ONNX_RUNTIME_ROOT "${CMAKE_BINARY_DIR}/onnxruntime")
set(ONNX_RUNTIME_URL "")
set(ONNX_RUNTIME_SO "")

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_LOWER)
if(WIN32)
    if(ARCH_LOWER STREQUAL "amd64" OR ARCH_LOWER STREQUAL "x86_64")
        set(ONNX_ARCH "win-x64")
    elseif(ARCH_LOWER MATCHES "arm64")
        set(ONNX_ARCH "win-arm64")
    else()
        message(FATAL_ERROR "Unsupported Windows architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

elseif(APPLE)
    if(ARCH_LOWER MATCHES "arm64")
        set(ONNX_ARCH "osx-arm64")
    elseif(ARCH_LOWER MATCHES "x86_64")
        set(ONNX_ARCH "osx-universal2")
    else()
        message(FATAL_ERROR "Unsupported macOS architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

elseif(UNIX)
    if(ARCH_LOWER MATCHES "aarch64" OR ARCH_LOWER MATCHES "arm64")
        set(ONNX_ARCH "linux-aarch64")
    elseif(ARCH_LOWER MATCHES "x86_64")
        set(ONNX_ARCH "linux-x64")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

set(ONNX_RUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_RUNTIME_VERSION}/onnxruntime-${ONNX_ARCH}-${ONNX_RUNTIME_VERSION}.tgz")

message(STATUS "Detected OS: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Detected Arch: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Resolved ONNX_ARCH: ${ONNX_ARCH}")

set(ONNX_RUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_RUNTIME_VERSION}/onnxruntime-${ONNX_ARCH}-${ONNX_RUNTIME_VERSION}.tgz")

# Create the directory if it doesn't exist
set(ONNX_ARCHIVE "${DEPS_DIR}/onnxruntime-${ONNX_ARCH}-${ONNX_RUNTIME_VERSION}.tgz")
set(ONNX_EXTRACT_DIR "${LIB_DIR}/onnxruntime")

file(MAKE_DIRECTORY ${DEPS_DIR})

# Download archive
if(NOT EXISTS ${ONNX_ARCHIVE})
    set(ONNX_RUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_RUNTIME_VERSION}/onnxruntime-${ONNX_ARCH}-${ONNX_RUNTIME_VERSION}.tgz")
    message(STATUS "Downloading ONNX Runtime from ${ONNX_RUNTIME_URL}")
    file(DOWNLOAD
        ${ONNX_RUNTIME_URL}
        ${ONNX_ARCHIVE}
        SHOW_PROGRESS
        STATUS _download_status
        LOG _download_log
    )
    list(GET _download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        message(FATAL_ERROR "Failed to download ONNX Runtime:\n${_download_log}")
    endif()
endif()

# Extract to build/ (i.e., ${CMAKE_BINARY_DIR})
if(NOT EXISTS ${ONNX_EXTRACT_DIR}/include)
    message(STATUS "Extracting ONNX Runtime archive to ${LIB_DIR} ...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${ONNX_ARCHIVE}
        WORKING_DIRECTORY ${LIB_DIR}
        RESULT_VARIABLE res
        ERROR_VARIABLE err
    )
    if(NOT res EQUAL 0)
        message(FATAL_ERROR "Failed to extract ONNX Runtime archive:\n${err}")
    endif()
    file(RENAME "${LIB_DIR}/onnxruntime-${ONNX_ARCH}-${ONNX_RUNTIME_VERSION}" "${ONNX_EXTRACT_DIR}")
endif()

# Setup include and lib paths
set(ONNX_RUNTIME_INCLUDE_DIR "${ONNX_EXTRACT_DIR}/include")
set(ONNX_RUNTIME_LIB_DIR "${ONNX_EXTRACT_DIR}/lib")

# Define shared lib name
if(WIN32)
    set(ONNX_SO "onnxruntime.dll")
elseif(APPLE)
    set(ONNX_SO "libonnxruntime.dylib")
else()
    set(ONNX_SO "libonnxruntime.so")
endif()
set(ONNX_RUNTIME_SHARED_LIB "${ONNX_RUNTIME_LIB_DIR}/${ONNX_SO}")

# Register as an imported target
add_library(onnxruntime SHARED IMPORTED GLOBAL)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNX_RUNTIME_SHARED_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNX_RUNTIME_INCLUDE_DIR}"
)
