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

# In freestanding SH builds, GCC's C++ include path can place libstdc++'s
# <math.h> compatibility wrapper ahead of the target C library header. That
# wrapper includes hosted-only <cmath>. Put the active target sysroot's C
# headers first for this target so <math.h> resolves to fxlibc/OpenLibm.
set(FXOGL_TARGET_SYSROOT "${CMAKE_SYSROOT}")
if(NOT FXOGL_TARGET_SYSROOT)
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-sysroot
        OUTPUT_VARIABLE FXOGL_TARGET_SYSROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE FXOGL_SYSROOT_QUERY_RESULT
    )
    if(NOT FXOGL_SYSROOT_QUERY_RESULT EQUAL 0)
        set(FXOGL_TARGET_SYSROOT "")
    endif()
endif()

if(FXOGL_TARGET_SYSROOT AND
   EXISTS "${FXOGL_TARGET_SYSROOT}/include/math.h")
    target_include_directories(fxogl BEFORE PRIVATE
        "${FXOGL_TARGET_SYSROOT}/include"
    )
    message(STATUS
        "fxOGL target C headers: ${FXOGL_TARGET_SYSROOT}/include")
elseif(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "fxOGL cross-build could not locate target C math header "
        "at '${FXOGL_TARGET_SYSROOT}/include/math.h'")
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
