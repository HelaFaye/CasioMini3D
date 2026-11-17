cmake_minimum_required(VERSION 3.15)
project(fxOGLHostTests LANGUAGES CXX)

enable_testing()

get_filename_component(FXOGL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(FXOGL_SRC "${FXOGL_ROOT}/src")
set(FXOGL_INCLUDE "${FXOGL_ROOT}/include")
set(HOST_STUBS "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

# Build the exact standalone library target applications link on-device.
add_subdirectory(${FXOGL_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/fxOGL)



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
