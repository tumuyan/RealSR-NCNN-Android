project(srmd-ncnn)

cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -Wl,-rpath=./")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wl,-rpath=./")

include_directories(${CMAKE_SOURCE_DIR}/${ANDROID_ABI}/include/ncnn)
add_library(ncnn SHARED IMPORTED)
set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/${ANDROID_ABI}/lib/libncnn.so)

add_executable(waifu2x-ncnn main.cpp waifu2x.cpp)

target_link_libraries(waifu2x-ncnn ncnn)