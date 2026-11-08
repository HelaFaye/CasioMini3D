cmake_minimum_required(VERSION 3.15)
project(fxOGLPackageConsumer LANGUAGES CXX)

find_package(fxOGL 0.1 CONFIG REQUIRED)

add_executable(fxogl_package_consumer main.cpp)
target_link_libraries(fxogl_package_consumer PRIVATE fxOGL::fxOGL)
