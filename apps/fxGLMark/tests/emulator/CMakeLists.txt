cmake_minimum_required(VERSION 3.15)
project(fxogl_emulator_regression LANGUAGES C CXX)

include(GenerateG3A)
include(Fxconv)
find_package(Gint 2.11 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/fxOGL"
    "${CMAKE_CURRENT_BINARY_DIR}/fxOGL"
)

add_executable(fxogl_emulator_regression main.cpp)
target_compile_options(fxogl_emulator_regression PRIVATE -Wall -Wextra -Os)
target_link_libraries(fxogl_emulator_regression PRIVATE fxOGL::fxOGL Gint::Gint stdc++)

generate_g3a(
    TARGET fxogl_emulator_regression
    OUTPUT "fxogl-emulator-regression.g3a"
    NAME "fxOGL GLES Regression"
    ICONS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/icon-uns.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/icon-sel.png"
)
