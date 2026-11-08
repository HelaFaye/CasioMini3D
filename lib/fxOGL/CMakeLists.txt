cmake_minimum_required(VERSION 3.15)
project(fxOGL LANGUAGES CXX)

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

target_compile_features(fxogl PUBLIC cxx_std_20)
target_compile_options(fxogl PRIVATE -Wall -Wextra -Os)

target_include_directories(fxogl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
