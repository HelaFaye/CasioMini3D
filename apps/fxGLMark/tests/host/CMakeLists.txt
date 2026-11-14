cmake_minimum_required(VERSION 3.15)
project(fxGLMarkHostTests LANGUAGES CXX)

enable_testing()

get_filename_component(APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(FXOGL_ROOT "${APP_ROOT}/../../lib/fxOGL" ABSOLUTE)

set(APP_SRC "${APP_ROOT}/src")
set(FXOGL_SRC "${FXOGL_ROOT}/src")
set(FXOGL_INCLUDE "${FXOGL_ROOT}/include")
set(HOST_STUBS "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

# Build the exact standalone library target that fxGLMark links on-device.
add_subdirectory(${FXOGL_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/fxOGL)

add_executable(triangle_tests
    test_triangle.cpp
    ${APP_SRC}/shapes/triangle.cpp
)
target_compile_features(triangle_tests PRIVATE cxx_std_20)
target_compile_options(triangle_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(triangle_tests PRIVATE
    ${HOST_STUBS} ${APP_SRC} ${FXOGL_INCLUDE} ${FXOGL_SRC}
)
target_link_libraries(triangle_tests PRIVATE fxOGL::fxOGL)
add_test(NAME triangle_rasterizer COMMAND triangle_tests)

add_executable(benchmark_tests
    test_bench.cpp
    ${APP_SRC}/bench.cpp
    ${APP_SRC}/shapes/triangle.cpp
)
target_compile_features(benchmark_tests PRIVATE cxx_std_20)
target_compile_options(benchmark_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(benchmark_tests PRIVATE
    ${HOST_STUBS} ${APP_SRC} ${FXOGL_INCLUDE} ${FXOGL_SRC}
)
target_link_libraries(benchmark_tests PRIVATE fxOGL::fxOGL)
add_test(NAME benchmark_registry COMMAND benchmark_tests)

add_executable(texture_tests test_texture.cpp)
target_compile_features(texture_tests PRIVATE cxx_std_20)
target_compile_options(texture_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(texture_tests PRIVATE ${FXOGL_INCLUDE} ${FXOGL_SRC})
target_link_libraries(texture_tests PRIVATE fxOGL::fxOGL)
add_test(NAME texture_sampler COMMAND texture_tests)

add_executable(gles_state_tests test_gles_state.cpp)
target_compile_features(gles_state_tests PRIVATE cxx_std_20)
target_compile_options(gles_state_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(gles_state_tests PRIVATE
    ${HOST_STUBS} ${FXOGL_INCLUDE} ${FXOGL_SRC}
)
target_link_libraries(gles_state_tests PRIVATE fxOGL::fxOGL)
add_test(NAME gles_context_state COMMAND gles_state_tests)

add_executable(gles_math_tests test_gles_math.cpp)
target_compile_features(gles_math_tests PRIVATE cxx_std_20)
target_compile_options(gles_math_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(gles_math_tests PRIVATE ${FXOGL_INCLUDE} ${FXOGL_SRC})
target_link_libraries(gles_math_tests PRIVATE fxOGL::fxOGL)
add_test(NAME gles_math_pipeline COMMAND gles_math_tests)

add_executable(gles_pipeline_tests test_gles_pipeline.cpp)
target_compile_features(gles_pipeline_tests PRIVATE cxx_std_20)
target_compile_options(gles_pipeline_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(gles_pipeline_tests PRIVATE ${FXOGL_INCLUDE} ${FXOGL_SRC})
target_link_libraries(gles_pipeline_tests PRIVATE fxOGL::fxOGL)
add_test(NAME gles_software_pipeline COMMAND gles_pipeline_tests)

add_executable(glsl_tests test_glsl.cpp)
target_compile_features(glsl_tests PRIVATE cxx_std_20)
target_compile_options(glsl_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(glsl_tests PRIVATE ${FXOGL_INCLUDE} ${FXOGL_SRC})
target_link_libraries(glsl_tests PRIVATE fxOGL::fxOGL)
add_test(NAME glsl_subset_vm COMMAND glsl_tests)

add_executable(glmark2_tests
    test_glmark2.cpp
    ${APP_SRC}/glmark2/glmark2.cpp
)
target_compile_features(glmark2_tests PRIVATE cxx_std_20)
target_compile_options(glmark2_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(glmark2_tests PRIVATE
    ${HOST_STUBS} ${APP_SRC} ${FXOGL_INCLUDE} ${FXOGL_SRC}
)
target_link_libraries(glmark2_tests PRIVATE fxOGL::fxOGL)
add_test(NAME fxGLMark_port COMMAND glmark2_tests)

# Golden-frame regression: render every fxGLMark scene at 396x224 and compare
# hashes of the color and depth buffers with the stored reference. Floating
# point contraction is disabled so the hashes do not depend on whether the
# host compiler fuses multiply-adds (the fx-CG50 build never does).
target_compile_options(fxogl PRIVATE -ffp-contract=off)
add_executable(fxglmark_scenes
    ${APP_ROOT}/tests/profile/scenes.cpp
    ${APP_SRC}/glmark2/glmark2.cpp
)
target_compile_features(fxglmark_scenes PRIVATE cxx_std_20)
target_compile_options(fxglmark_scenes PRIVATE -Wall -Wextra -Werror -ffp-contract=off)
target_include_directories(fxglmark_scenes PRIVATE
    ${HOST_STUBS} ${APP_SRC} ${FXOGL_INCLUDE} ${FXOGL_SRC}
)
target_link_libraries(fxglmark_scenes PRIVATE fxOGL::fxOGL)
add_test(NAME fxglmark_golden_frames
    COMMAND fxglmark_scenes --check ${APP_ROOT}/tests/profile/golden_frames.txt)

# Compile the real calculator entry point against small Gint/fxlibc stubs.
add_library(fxglmark_main_compile OBJECT ${APP_SRC}/main.cpp)
target_compile_features(fxglmark_main_compile PRIVATE cxx_std_20)
target_compile_options(fxglmark_main_compile PRIVATE -Wall -Wextra -Werror)
target_include_directories(fxglmark_main_compile PRIVATE
    ${HOST_STUBS} ${APP_SRC} ${FXOGL_INCLUDE} ${FXOGL_SRC}
)

# Exercise the install/export interface from a separate CMake project. This
# catches missing installed headers, package metadata and exported link names.
add_test(
    NAME fxogl_installed_package_consumer
    COMMAND ${CMAKE_COMMAND}
        -DFXOGL_INSTALL_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DFXOGL_PACKAGE_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/package-prefix
        -DFXOGL_PACKAGE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_consumer
        -DFXOGL_PACKAGE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/package-consumer-build
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_installed_package.cmake
)
