cmake_minimum_required(VERSION 3.16)

set(VERSION_FILE    "cpplib.version")
set(VERSION_FILE_OUT    "${CMAKE_BINARY_DIR}/version.def")

set(LIB_TARGETS_NAME    "Rbel12b_cpplibTargets")

file(READ ${VERSION_FILE} VERSION_STRING)

project("Rbel12b_cpplib" VERSION ${VERSION_STRING} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(lib/ProcessUtils)

# Export all targets
install(
    EXPORT ${LIB_TARGETS_NAME}
    FILE ${LIB_TARGETS_NAME}.cmake
    NAMESPACE Rbel12b-cpplib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Rbel12b-cpplib
)

# Config + version files
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/Rbel12b-cpplibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Rbel12b-cpplibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Rbel12b-cpplibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Rbel12b-cpplib
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/Rbel12b-cpplibConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/Rbel12b-cpplibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Rbel12b-cpplib
)