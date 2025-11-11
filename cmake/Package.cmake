set(PACKAGES_DIR "${CMAKE_SOURCE_DIR}/package")
set(APP_VERSION "0.2.5")

set(PACKAGE_ARCH "")

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_LOWER)
if (ARCH_LOWER MATCHES "x86_64" OR ARCH_LOWER STREQUAL "amd64")
    set(PACKAGE_ARCH "x64")
elseif(ARCH_LOWER MATCHES "aarch64" OR ARCH_LOWER MATCHES "arm64")
    set(PACKAGE_ARCH "arm64")
endif()

if(WIN32)
    set(PACKAGE_ARCH "win-${PACKAGE_ARCH}")
elseif(APPLE)
    set(PACKAGE_ARCH "macos-universal")
elseif(UNIX)
    set(PACKAGE_ARCH "linux-${PACKAGE_ARCH}")
endif()

string(TOLOWER "${PROJECT_NAME}" APP_NAME)

if(NOT PACKAGE_NAME)
    if (WIN32 AND STATIC_BUILD)
        set(PACKAGE_NAME "${APP_NAME}-static-${APP_VERSION}-${PACKAGE_ARCH}")
    else()
        set(PACKAGE_NAME "${APP_NAME}-${APP_VERSION}-${PACKAGE_ARCH}")
    endif()
endif()

set(PACKAGE_DIR "${PACKAGES_DIR}/${PACKAGE_NAME}")

add_custom_target(package
    COMMENT "Packaging deej-ai"
    DEPENDS deej-ai
)

add_custom_target(package_zip
    COMMENT "Packaging deej-ai into ZIP"
    DEPENDS deej-ai package
)

file(MAKE_DIRECTORY ${PACKAGES_DIR})

set(PACKAGE_LICENSE_DIR "${PACKAGE_DIR}/LICENSES")
add_custom_command(
    TARGET package POST_BUILD
    WORKING_DIRECTORY ${PACKAGES_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BIN_DIR} "${PACKAGE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/LICENSES" "${PACKAGE_DIR}/share/LICENSES"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${PACKAGE_DIR}/share/LICENSE"
)

# onnxruntime library
if(WIN32)
    # create the onnx license directory
    add_custom_command(
        TARGET package POST_BUILD
        WORKING_DIRECTORY ${PACKAGES_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}/share/LICENSES/onnxruntime"
    )
    if (STATIC_BUILD)
        add_custom_command(
            TARGET package POST_BUILD
            WORKING_DIRECTORY ${PACKAGES_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy "${ONNX_STATIC_DIR}/LICENSE" "${PACKAGE_DIR}/share/LICENSES/onnxruntime/LICENSE"
            COMMAND ${CMAKE_COMMAND} -E copy "${ONNX_STATIC_DIR}/ThirdPartyNotices.txt" "${PACKAGE_DIR}/share/LICENSES/onnxruntime/ThirdPartyNotices.txt"
            COMMAND ${CMAKE_COMMAND} -E copy "${ONNX_STATIC_DIR}/VERSION_NUMBER" "${PACKAGE_DIR}/share/LICENSES/onnxruntime/VERSION_NUMBER"
            COMMAND ${CMAKE_COMMAND} -E copy "${ONNX_STATIC_DIR}/README.md" "${PACKAGE_DIR}/share/LICENSES/onnxruntime/README.md"
        )
    else()
        # copy the onnx license files
        file(GLOB LICENSE_FILES "${ONNX_EXTRACT_DIR}/*")
        foreach(file_path IN LISTS LICENSE_FILES)
            add_custom_command(
                TARGET package POST_BUILD
                WORKING_DIRECTORY ${PACKAGES_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy "${file_path}" "${PACKAGE_DIR}/share/LICENSES/onnxruntime"
            )
        endforeach()
    endif()
else()
    # use this instead of copy_directory to keep the symlinks
    add_custom_command(
        TARGET package POST_BUILD
        WORKING_DIRECTORY ${PACKAGES_DIR}
        COMMAND cp -a "${LIB_DIR}" "${PACKAGE_DIR}/lib/"
    )
endif()


# eigen licenses
file(GLOB LICENSE_FILES "${eigen_SOURCE_DIR}/COPYING.*")
foreach(file_path IN LISTS LICENSE_FILES)
    get_filename_component(filename "${file_path}" NAME)
    add_custom_command(
        TARGET package POST_BUILD
        WORKING_DIRECTORY ${PACKAGES_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy "${file_path}" "${PACKAGE_DIR}/share/LICENSES/eigen/${filename}"
    )
endforeach()
add_custom_command(
    TARGET package POST_BUILD
    WORKING_DIRECTORY ${PACKAGES_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy "${eigen_SOURCE_DIR}/README.md" "${PACKAGE_DIR}/share/LICENSES/eigen/README.md"
)

# zip the package
add_custom_command(
    TARGET package_zip POST_BUILD
    WORKING_DIRECTORY ${PACKAGES_DIR}
    COMMAND ${CMAKE_COMMAND} -E tar cvf "${PACKAGE_DIR}.zip" --format=zip ${PACKAGE_DIR}
)
