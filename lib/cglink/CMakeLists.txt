cmake_minimum_required(VERSION 3.16)
project(cglink C)

# Portable calculator <-> ESP32 link library: framing, pak index, deflate
# decoder, page cache and page streaming. Builds for the host, the ESP32
# (ESP-IDF) and the fx-CG50 (gint); no allocation, no floating point.
add_library(cglink STATIC
    src/frame.c
    src/inflate.c
    src/pak.c
    src/cache.c
    src/pages.c)
target_include_directories(cglink PUBLIC include)
set_target_properties(cglink PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cglink PRIVATE -Wall -Wextra -Werror)
endif()

if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
    find_package(ZLIB REQUIRED)
    add_executable(cglink_tests tests/test_cglink.c)
    target_link_libraries(cglink_tests PRIVATE cglink ZLIB::ZLIB)
    add_test(NAME cglink_tests COMMAND cglink_tests)

    add_executable(cglink_sim tools/cglink_sim.c)
    target_link_libraries(cglink_sim PRIVATE cglink)
endif()
