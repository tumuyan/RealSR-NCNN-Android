project(realsr-ncnn)

cmake_minimum_required(VERSION 3.4.1)
set(ncnn_DIR ${CMAKE_SOURCE_DIR}/${ANDROID_ABI}/lib/cmake/ncnn)
find_package(ncnn REQUIRED)

add_executable(realsr-ncnn
        main.cpp
        realsr.cpp
        )

target_link_libraries(realsr-ncnn ncnn)
