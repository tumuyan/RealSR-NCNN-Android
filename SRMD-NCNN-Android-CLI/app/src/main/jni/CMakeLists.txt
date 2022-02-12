project(srmd-ncnn)

cmake_minimum_required(VERSION 3.4.1)
set(ncnn_DIR ${CMAKE_SOURCE_DIR}/${ANDROID_ABI}/lib/cmake/ncnn)
find_package(ncnn REQUIRED)

add_executable(srmd-ncnn main.cpp srmd.cpp)

target_link_libraries(srmd-ncnn ncnn)