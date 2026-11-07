cmake_minimum_required(VERSION 3.15)
project(CasioMini3DHostTests LANGUAGES CXX)

enable_testing()

add_executable(triangle_tests
    test_triangle.cpp
    ../../src/shapes/triangle.cpp
    ../../src/gfx/texture.cpp
)

target_compile_features(triangle_tests PRIVATE cxx_std_20)
target_compile_options(triangle_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(triangle_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

add_test(NAME triangle_rasterizer COMMAND triangle_tests)

add_executable(benchmark_tests
    test_bench.cpp
    ../../src/bench.cpp
    ../../src/shapes/triangle.cpp
    ../../src/gfx/texture.cpp
)

target_compile_features(benchmark_tests PRIVATE cxx_std_20)
target_compile_options(benchmark_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(benchmark_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

add_test(NAME benchmark_registry COMMAND benchmark_tests)

add_executable(texture_tests
    test_texture.cpp
    ../../src/gfx/texture.cpp
)

target_compile_features(texture_tests PRIVATE cxx_std_20)
target_compile_options(texture_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME texture_sampler COMMAND texture_tests)
