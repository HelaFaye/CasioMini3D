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

add_executable(gles_state_tests
    test_gles_state.cpp
    ../../src/gles/gles_context.cpp
    ../../src/gles/math.cpp
    ../../src/gles/pipeline.cpp
    ../../src/gles/program.cpp
    ../../src/gles/glsl.cpp
    ../../src/gfx/texture.cpp
)

target_compile_features(gles_state_tests PRIVATE cxx_std_20)
target_compile_options(gles_state_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(gles_state_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
add_test(NAME gles_context_state COMMAND gles_state_tests)

add_executable(gles_math_tests
    test_gles_math.cpp
    ../../src/gles/math.cpp
)

target_compile_features(gles_math_tests PRIVATE cxx_std_20)
target_compile_options(gles_math_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME gles_math_pipeline COMMAND gles_math_tests)

add_executable(gles_pipeline_tests
    test_gles_pipeline.cpp
    ../../src/gles/math.cpp
    ../../src/gles/pipeline.cpp
)

target_compile_features(gles_pipeline_tests PRIVATE cxx_std_20)
target_compile_options(gles_pipeline_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(gles_pipeline_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
add_test(NAME gles_software_pipeline COMMAND gles_pipeline_tests)

add_executable(glsl_tests
    test_glsl.cpp
    ../../src/gles/glsl.cpp
)

target_compile_features(glsl_tests PRIVATE cxx_std_20)
target_compile_options(glsl_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME glsl_subset_vm COMMAND glsl_tests)

add_executable(glmark2_tests
    test_glmark2.cpp
    ../../src/glmark2/glmark2.cpp
    ../../src/gles/gles_context.cpp
    ../../src/gles/math.cpp
    ../../src/gles/pipeline.cpp
    ../../src/gles/program.cpp
    ../../src/gles/glsl.cpp
    ../../src/gfx/texture.cpp
)

target_compile_features(glmark2_tests PRIVATE cxx_std_20)
target_compile_options(glmark2_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(glmark2_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
add_test(NAME fxglmark2_port COMMAND glmark2_tests)

# Compile the real calculator entry point against small Gint/fxlibc stubs.
# This catches menu/key/API integration errors in the host CI build.
add_library(addin_main_compile OBJECT ../../src/main.cpp)
target_compile_features(addin_main_compile PRIVATE cxx_std_20)
target_compile_options(addin_main_compile PRIVATE -Wall -Wextra -Werror)
target_include_directories(addin_main_compile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
