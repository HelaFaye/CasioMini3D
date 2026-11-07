cmake_minimum_required(VERSION 3.15)
project(CasioMini3DHostTests LANGUAGES CXX)

enable_testing()

add_executable(triangle_tests
    test_triangle.cpp
    ../../src/shapes/triangle.cpp
)

target_compile_features(triangle_tests PRIVATE cxx_std_20)
target_compile_options(triangle_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(triangle_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

add_test(NAME triangle_rasterizer COMMAND triangle_tests)
