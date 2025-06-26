set(PACKAGES_DIR "${CMAKE_SOURCE_DIR}/package")
set(APP_VERSION "0.1.0")

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
    set(PACKAGE_ARCH "macos-${PACKAGE_ARCH}")
elseif(UNIX)
    set(PACKAGE_ARCH "linux-${PACKAGE_ARCH}")
endif()

string(TOLOWER "${PROJECT_NAME}" APP_NAME)
set(PACKAGE_DIR "${PACKAGES_DIR}/${APP_NAME}-${APP_VERSION}-${PACKAGE_ARCH}")

add_custom_target(package_zip
    COMMENT "Packaging deej-ai into ZIP"
    DEPENDS deej-ai
)

file(MAKE_DIRECTORY ${PACKAGES_DIR})

set(PACKAGE_LICENSE_DIR "${PACKAGE_DIR}/LICENSES")
add_custom_command(
    TARGET package_zip POST_BUILD
    WORKING_DIRECTORY ${PACKAGES_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BIN_DIR} "${PACKAGE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/LICENSES" "${PACKAGE_DIR}/share/LICENSES"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${PACKAGE_DIR}/share/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E tar cfv "${PACKAGE_DIR}.zip" --format=zip ${PACKAGE_DIR}
)

if(WIN32)
    add_custom_command(
        TARGET package_zip POST_BUILD
        WORKING_DIRECTORY ${PACKAGES_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIB_DIR} "${PACKAGE_DIR}/lib"
    )
# use this instead of copy_directory to keep the symlinks
else()
    add_custom_command(
        TARGET package_zip POST_BUILD
        WORKING_DIRECTORY ${PACKAGES_DIR}
        COMMAND cp -a "${LIB_DIR}" "${PACKAGE_DIR}/lib/"
    )
endif()


# eigen licenses
file(GLOB LICENSE_FILES "${eigen_SOURCE_DIR}/COPYING.*")
foreach(file_path IN LISTS LICENSE_FILES)
    get_filename_component(filename "${file_path}" NAME)
    add_custom_command(
        TARGET package_zip POST_BUILD
        WORKING_DIRECTORY ${PACKAGES_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy "${file_path}" "${PACKAGE_DIR}/share/LICENSES/eigen/${filename}"
    )
endforeach()
add_custom_command(
    TARGET package_zip POST_BUILD
    WORKING_DIRECTORY ${PACKAGES_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy "${eigen_SOURCE_DIR}/README.md" "${PACKAGE_DIR}/share/LICENSES/eigen/README.md"
)
