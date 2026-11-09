cmake_minimum_required(VERSION 3.15)
project(fxOGL VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Portable software-renderer core. The calculator application supplies the
# Gint framebuffer/presentation integration; this library itself has no Gint
# dependency.
add_library(fxogl STATIC
    src/gfx/texture.cpp
    src/gles/gles_context.cpp
    src/gles/math.cpp
    src/gles/pipeline.cpp
    src/gles/glsl.cpp
    src/gles/program.cpp
)

add_library(fxOGL::fxOGL ALIAS fxogl)
set_target_properties(fxogl PROPERTIES EXPORT_NAME fxOGL)

target_compile_features(fxogl PUBLIC cxx_std_20)
target_compile_options(fxogl PRIVATE -Wall -Wextra -Os)

# In freestanding SH builds, GCC's C++ include path can put libstdc++'s
# <math.h> compatibility wrapper ahead of the target C library header. Ask
# fxSDK for its canonical target include directory rather than reconstructing
# it from GCC's sysroot layout (which differs between toolchain releases).
set(FXOGL_TARGET_C_INCLUDE_DIR "")

execute_process(
    COMMAND fxsdk path include
    OUTPUT_VARIABLE FXOGL_FXSDK_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE FXOGL_FXSDK_INCLUDE_RESULT
)

if(FXOGL_FXSDK_INCLUDE_RESULT EQUAL 0 AND
   FXOGL_FXSDK_INCLUDE_DIR AND
   EXISTS "${FXOGL_FXSDK_INCLUDE_DIR}/math.h")
    set(FXOGL_TARGET_C_INCLUDE_DIR "${FXOGL_FXSDK_INCLUDE_DIR}")
endif()

# Fall back to compiler-reported locations for non-fxSDK SH toolchains.
if(NOT FXOGL_TARGET_C_INCLUDE_DIR AND
   CMAKE_SYSROOT AND EXISTS "${CMAKE_SYSROOT}/include/math.h")
    set(FXOGL_TARGET_C_INCLUDE_DIR "${CMAKE_SYSROOT}/include")
endif()

execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -print-sysroot
    OUTPUT_VARIABLE FXOGL_COMPILER_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE FXOGL_SYSROOT_QUERY_RESULT
)
execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
    OUTPUT_VARIABLE FXOGL_TARGET_TRIPLE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE FXOGL_TRIPLE_QUERY_RESULT
)

if(NOT FXOGL_TARGET_C_INCLUDE_DIR AND
   FXOGL_SYSROOT_QUERY_RESULT EQUAL 0 AND
   FXOGL_TRIPLE_QUERY_RESULT EQUAL 0 AND
   FXOGL_COMPILER_SYSROOT AND FXOGL_TARGET_TRIPLE)
    set(FXOGL_TRIPLE_C_INCLUDE_DIR
        "${FXOGL_COMPILER_SYSROOT}/${FXOGL_TARGET_TRIPLE}/include")
    if(EXISTS "${FXOGL_TRIPLE_C_INCLUDE_DIR}/math.h")
        set(FXOGL_TARGET_C_INCLUDE_DIR "${FXOGL_TRIPLE_C_INCLUDE_DIR}")
    elseif(EXISTS "${FXOGL_COMPILER_SYSROOT}/include/math.h")
        set(FXOGL_TARGET_C_INCLUDE_DIR "${FXOGL_COMPILER_SYSROOT}/include")
    endif()
endif()

if(FXOGL_TARGET_C_INCLUDE_DIR)
    target_include_directories(fxogl BEFORE PRIVATE
        "${FXOGL_TARGET_C_INCLUDE_DIR}"
    )
    message(STATUS
        "fxOGL target C headers: ${FXOGL_TARGET_C_INCLUDE_DIR}")
elseif(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "fxOGL cross-build could not locate the target C math header. "
        "On fxSDK builds, verify that 'fxsdk path include' resolves to "
        "the target's include directory.")
endif()

target_include_directories(fxogl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(FXOGL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/fxOGL")

configure_package_config_file(
    cmake/fxOGLConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/fxOGLConfig.cmake"
    INSTALL_DESTINATION "${FXOGL_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fxOGLConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS fxogl
    EXPORT fxOGLTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(EXPORT fxOGLTargets
    FILE fxOGLTargets.cmake
    NAMESPACE fxOGL::
    DESTINATION "${FXOGL_INSTALL_CMAKEDIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/fxOGLConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fxOGLConfigVersion.cmake"
    DESTINATION "${FXOGL_INSTALL_CMAKEDIR}"
)
