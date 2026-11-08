# Configure with fxsdk build-cg, which supplies the SH toolchain and fxSDK modules.

cmake_minimum_required(VERSION 3.15)
project(fxGLMark LANGUAGES C CXX)

include(GenerateG1A)
include(GenerateG3A)
include(Fxconv)

find_package(Gint 2.11 REQUIRED)
find_package(LibProf 2.4 REQUIRED)

add_compile_options(${CPPFLAGS} ${CFLAGS} -Wall -Wextra -Os -std=c++20 -fno-freestanding)

# Build the reusable graphics library as a separate target.
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/fxOGL
    ${CMAKE_CURRENT_BINARY_DIR}/fxOGL
)

set(APP_SOURCES
    src/main.cpp
    src/bench.cpp
    src/world.cpp
    src/display.cpp
    src/shapes/parallelogram.cpp
    src/shapes/rectangle.cpp
    src/shapes/triangle.cpp
)

# Shared assets, fx-9860G-only assets and fx-CG-50-only assets.
set(ASSETS)
set(ASSETS_fx)
set(ASSETS_cg
    #/*<IMG_CG>*/
    assets/Textures-16.png
    #/*</IMG_CG>*/
)

fxconv_declare_assets(${ASSETS} ${ASSETS_fx} ${ASSETS_cg} WITH_METADATA)

add_executable(fxglmark ${APP_SOURCES} ${ASSETS} ${ASSETS_${FXSDK_PLATFORM}})
target_compile_options(fxglmark PRIVATE -Wall -Wextra -Os)
target_include_directories(fxglmark PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/fxOGL/src
)
target_link_libraries(fxglmark PRIVATE
    fxOGL::fxOGL
    Gint::Gint
    LibProf::LibProf
    stdc++
)

set(NAME_APP "fxGLMark")
generate_g3a(TARGET fxglmark OUTPUT "${NAME_APP}.g3a"
    NAME "fxGLMark" ICONS assets/icon-uns.png assets/icon-sel.png)
