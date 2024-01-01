project(resize-ncnn)

cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -s -Wall -g -ggdb -Wl,-rpath=./")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wl,-rpath=./")


if (WIN32)
    if (CMAKE_CL_64)
        set(TARG_CPU x64)
    else ()
        set(TARG_CPU x86)
    endif ()

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../Lib/valkan/Include)

    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-windows-vs2019-shared)

    add_library(ncnn SHARED IMPORTED)
    include_directories(${ncnn_DIR}/${TARG_CPU}/include/ncnn)
    set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION
            ${ncnn_DIR}/${TARG_CPU}/lib/ncnn.lib)
else ()
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-android-vulkan-shared)

    add_library(ncnn SHARED IMPORTED)
    include_directories(${ncnn_DIR}/${ANDROID_ABI}/include/ncnn)
    set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION
            ${ncnn_DIR}/${ANDROID_ABI}/lib/libncnn.so)
endif ()
#set(libwebp_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libwebp)
set(libwebp_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/libwebp)
#include_directories(${CMAKE_CURRENT_BINARY_DIR})
option(USE_SYSTEM_WEBP "build with system libwebp" OFF)
if (NOT USE_SYSTEM_WEBP)
    # build libwebp library
    if(NOT EXISTS "${libwebp_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Not find libwebp CMakeLists.txt!")
    endif()

    option(WEBP_ENABLE_SIMD "" ON)
    option(WEBP_BUILD_ANIM_UTILS "" OFF)
    option(WEBP_BUILD_CWEBP "" OFF)
    option(WEBP_BUILD_DWEBP "" OFF)
    option(WEBP_BUILD_GIF2WEBP "" OFF)
    option(WEBP_BUILD_IMG2WEBP "" OFF)
    option(WEBP_BUILD_VWEBP "" OFF)
    option(WEBP_BUILD_WEBPINFO "" OFF)
    option(WEBP_BUILD_WEBPMUX "" OFF)
    option(WEBP_BUILD_EXTRAS "" OFF)
    option(WEBP_BUILD_WEBP_JS "" OFF)
    option(WEBP_NEAR_LOSSLESS "" OFF)
    option(WEBP_ENABLE_SWAP_16BIT_CSP "" OFF)

    add_subdirectory(${libwebp_DIR}  ${CMAKE_BINARY_DIR}/libwebp)
    include_directories(${CMAKE_BINARY_DIR}/libwebp/src)
endif ()


add_executable(resize-ncnn
        main.cpp
        )

target_link_libraries(resize-ncnn webp ncnn)
