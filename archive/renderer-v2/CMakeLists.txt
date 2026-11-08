# Configure with [fxsdk build-fx] or [fxsdk build-cg], which provide the
# toolchain file and module path of the fxSDK

cmake_minimum_required(VERSION 3.15)
project(3D)

include(GenerateG1A)
include(GenerateG3A)
include(Fxconv)
find_package(Gint 2.11 REQUIRED)

add_compile_options(${CPPFLAGS} ${CFLAGS} -Wall -Wextra -Os -std=c++20 -fno-freestanding)


set(SOURCES
    src/main.cpp
    src/world.cpp
    src/display.cpp
)

# Shared assets, fx-9860G-only assets and fx-CG-50-only assets
set(ASSETS)
set(ASSETS_fx)

set(ASSETS_cg
  #/*<IMG_CG>*/
assets/Textures-16.png
#/*</IMG_CG>*/
  # ...
)

fxconv_declare_assets(${ASSETS} ${ASSETS_fx} ${ASSETS_cg} WITH_METADATA)

add_executable(myaddin ${SOURCES} ${ASSETS} ${ASSETS_${FXSDK_PLATFORM}})
target_compile_options(myaddin PRIVATE -Wall -Wextra -Os)
target_link_libraries(myaddin Gint::Gint "${CMAKE_CURRENT_SOURCE_DIR}/libsupc++.a")

set(NAME_APP "3D")


generate_g3a(TARGET myaddin OUTPUT "${NAME_APP}.g3a"
    NAME "3D" ICONS assets/icon-uns.png assets/icon-sel.png)

