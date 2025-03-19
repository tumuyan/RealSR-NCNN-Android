project(mnnsr)

cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)



if(MSVC)  # Visual Studio
    message(STATUS "==Configuring for Visual Studio==")

    if("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
        set(TARGET_ARCH "x64")
    elseif("${CMAKE_GENERATOR_PLATFORM}" MATCHES "ARM64")
        set(TARGET_ARCH "ARM64")
    elseif("${CMAKE_GENERATOR}" MATCHES "ARM")
        set(TARGET_ARCH "ARM")
    elseif("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        set(TARGET_ARCH "x64")
    else()
        set(TARGET_ARCH x86)
    endif()

    # 设置目标属性，指定 MNN 库的路径
    set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_windows_x64_cpu_opencl)
    find_library(MNN_LIB mnn HINTS "${mnn_DIR}/lib/${TARGET_ARCH}/Release/Dynamic/MD")

#    add_library(MNN_Vulkan SHARED IMPORTED)
#    set_target_properties(MNN_Vulkan PROPERTIES IMPORTED_LOCATION
#            ${mnn_DIR}/${ANDROID_ABI}/libMNN_Vulkan.so)

    set(OpenCV_DIR "C:/Lib/opencv/build")
    
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-windows-vs2022-shared)
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/${TARGET_ARCH}/lib)
 #  set(ncnn_LIB ${ncnn_DIR}/${TARGET_ARCH}/lib/ncnn.lib)


else()  # Android Studio
    message(STATUS "Configuring for Android Studio")
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -s -Wall -g -ggdb -Wl,-rpath=./")
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wl,-rpath=./")

    set(TARGET_ARCH ${ANDROID_ABI})

    # 设置目标属性，指定 MNN 库的路径
    set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_android)
    
    include_directories(${mnn_DIR}/include)
#include_directories(${mnn_DIR}/${ANDROID_ABI}/include/MNN)
    find_library(MNN_LIB mnn HINTS "${mnn_DIR}/${TARGET_ARCH}")

    add_library(MNN SHARED IMPORTED)
    set_target_properties(MNN PROPERTIES IMPORTED_LOCATION
            ${mnn_DIR}/${ANDROID_ABI}/libMNN.so)

    add_library(c++_shared SHARED IMPORTED)
    set_target_properties(c++_shared PROPERTIES IMPORTED_LOCATION
            ${mnn_DIR}/${ANDROID_ABI}/libc++_shared.so)

    add_library(MNN_CL SHARED IMPORTED)
    set_target_properties(MNN_CL PROPERTIES IMPORTED_LOCATION
            ${mnn_DIR}/${ANDROID_ABI}/libMNN_CL.so)

    add_library(MNN_Vulkan SHARED IMPORTED)
    set_target_properties(MNN_Vulkan PROPERTIES IMPORTED_LOCATION
            ${mnn_DIR}/${ANDROID_ABI}/libMNN_Vulkan.so)

    set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/OpenCV-android-sdk/sdk/native/jni/)
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-android-vulkan-shared)
#    set(ncnn_LIB ${ncnn_DIR}/${ANDROID_ABI}/lib/libncnn.so)

endif()

message(STATUS "TARGET_ARCH: ${TARGET_ARCH}")
include_directories(${mnn_DIR}/include)
#include_directories(${mnn_DIR}/${ANDROID_ABI}/include/MNN)



if(EXISTS ${MNN_LIB})
    message(STATUS "find mnn: ${MNN_LIB}")
else()
    message(STATUS "mnn library not found!")
    set(MNN_LIB "${mnn_DIR}/${ANDROID_ABI}/libMNN.so")
endif()
message(STATUS "find mnn result: ${MNN_LIB}")


add_library(ncnn SHARED IMPORTED)
include_directories(${ncnn_DIR}/${TARGET_ARCH}/include/ncnn)
# set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION  ${ncnn_LIB})


message(STATUS "find ncnn in: ${ncnn_DIR}/${TARGET_ARCH}/lib")
find_library(NCNN_LIB NAMES  ncnn libncnn PATHS "${ncnn_DIR}/${TARGET_ARCH}/lib")
if(EXISTS ${NCNN_LIB})
    message(STATUS "Found ncnn: ${NCNN_LIB}")
else()
    message(STATUS "ncnn library not found!")
    set(NCNN_LIB "${ncnn_DIR}/${TARGET_ARCH}/lib/libncnn.so")
endif()
message(STATUS "find ncnn result: ${NCNN_LIB}")

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_DIR}/include)

set(libwebp_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/libwebp)



#include_directories(${CMAKE_CURRENT_BINARY_DIR})
option(USE_SYSTEM_WEBP "build with system libwebp" OFF)
if(NOT USE_SYSTEM_WEBP)
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
endif()


add_executable(${PROJECT_NAME}  main.cpp  mnnsr.cpp)

target_link_libraries(${PROJECT_NAME} webp ${MNN_LIB} ${OpenCV_LIBS}  ${NCNN_LIB})

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../RealSR-NCNN-Android-GUI/app/src/main/assets/realsr
)