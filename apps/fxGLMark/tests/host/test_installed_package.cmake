if(NOT DEFINED FXOGL_INSTALL_BUILD_DIR OR
   NOT DEFINED FXOGL_PACKAGE_PREFIX OR
   NOT DEFINED FXOGL_PACKAGE_SOURCE_DIR OR
   NOT DEFINED FXOGL_PACKAGE_BUILD_DIR)
    message(FATAL_ERROR "Missing fxOGL package smoke-test paths")
endif()

file(REMOVE_RECURSE
    "${FXOGL_PACKAGE_PREFIX}"
    "${FXOGL_PACKAGE_BUILD_DIR}"
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install
        "${FXOGL_INSTALL_BUILD_DIR}"
        --prefix "${FXOGL_PACKAGE_PREFIX}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "fxOGL install failed (code ${install_result})\n"
        "${install_stdout}\n${install_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${FXOGL_PACKAGE_SOURCE_DIR}"
        -B "${FXOGL_PACKAGE_BUILD_DIR}"
        "-DCMAKE_PREFIX_PATH=${FXOGL_PACKAGE_PREFIX}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "fxOGL package consumer configure failed (code ${configure_result})\n"
        "${configure_stdout}\n${configure_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${FXOGL_PACKAGE_BUILD_DIR}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "fxOGL package consumer build failed (code ${build_result})\n"
        "${build_stdout}\n${build_stderr}")
endif()

message(STATUS "fxOGL installed-package consumer: PASS")
