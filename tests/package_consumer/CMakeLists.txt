cmake_minimum_required(VERSION 3.24)

project(houio_package_consumer LANGUAGES CXX)

find_package(houio 0.2 CONFIG REQUIRED)

add_executable(houio_package_consumer main.cpp)
target_link_libraries(houio_package_consumer PRIVATE houio::houio)
target_compile_features(houio_package_consumer PRIVATE cxx_std_20)
