project(mnnsr-ncnn)

cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)


if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    message(STATUS "Building for Android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building for Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building for Linux")
else()
    message(WARNING "Unknown system: ${CMAKE_SYSTEM_NAME}")
endif()

if (MSVC)  # Visual Studio
    message(STATUS "==Configuring for Visual Studio==")
    add_definitions(-DNOMINMAX)

    if ("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
        set(TARGET_ARCH "x64")
    elseif ("${CMAKE_GENERATOR_PLATFORM}" MATCHES "ARM64")
        set(TARGET_ARCH "ARM64")
    elseif ("${CMAKE_GENERATOR}" MATCHES "ARM")
        set(TARGET_ARCH "ARM")
    elseif ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        set(TARGET_ARCH "x64")
    else ()
        set(TARGET_ARCH x86)
    endif ()

    # 设置目标属性，指定 MNN 库的路径
    # set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_windows_x64_cpu_opencl)
    set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_windows)
    find_library(MNN_LIB mnn HINTS "${mnn_DIR}/lib/${TARGET_ARCH}/Release/Dynamic/MT")

    set(OpenCV_DIR "C:/Lib/opencv/build/x64/vc16/lib")
    # set(OpenCV_DIR "C:/Lib/opencv/build")

    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-windows-vs2022-shared)
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/${TARGET_ARCH}/lib)
    #  set(ncnn_LIB ${ncnn_DIR}/${TARGET_ARCH}/lib/ncnn.lib)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "==Configuring for Linux==")
    add_link_options(-Wl,--disable-new-dtags)

    set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/MNN)
    find_library(MNN_LIB MNN HINTS "${mnn_DIR}/libs")

    # set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_linux_x64)
    # find_library(MNN_LIB MNN HINTS "${mnn_DIR}/lib/Release")
    # include_directories(${mnn_DIR}/include)
    message(STATUS "find mnn dir: ${mnn_DIR}")
    message(STATUS "find mnn: ${MNN_LIB}")
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-ubuntu-shared)
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/lib)
    #  set(ncnn_LIB ${ncnn_DIR}/${TARGET_ARCH}/lib/ncnn.lib)

else ()  # Android Studio
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

endif ()

message(STATUS "TARGET_ARCH: ${TARGET_ARCH}")
include_directories(${mnn_DIR}/include)
#include_directories(${mnn_DIR}/${ANDROID_ABI}/include/MNN)


if (EXISTS ${MNN_LIB})
    message(STATUS "find mnn: ${MNN_LIB}")
else ()
    message(STATUS "mnn library not found!")
    set(MNN_LIB "${mnn_DIR}/${ANDROID_ABI}/libMNN.so")
endif ()
message(STATUS "find mnn result: ${MNN_LIB}")


add_library(ncnn SHARED IMPORTED)
include_directories(${ncnn_DIR}/${TARGET_ARCH}/include/ncnn)
# set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION  ${ncnn_LIB})


message(STATUS "find ncnn in: ${ncnn_DIR}/${TARGET_ARCH}/lib")
find_library(NCNN_LIB NAMES ncnn libncnn PATHS "${ncnn_DIR}/${TARGET_ARCH}/lib")
if (EXISTS ${NCNN_LIB})
    message(STATUS "Found ncnn: ${NCNN_LIB}")
else ()
    message(STATUS "ncnn library not found!")
    set(NCNN_LIB "${ncnn_DIR}/${TARGET_ARCH}/lib/libncnn.so")
endif ()
message(STATUS "find ncnn result: ${NCNN_LIB}")

set(OpenCV_STATIC ON)
find_package(OpenCV REQUIRED)
# include_directories(${OpenCV_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

# 获取 OpenCV 库文件所在路径
get_target_property(OpenCV_CORE_DLL_PATH opencv_core IMPORTED_LOCATION_RELEASE)
if(OpenCV_CORE_DLL_PATH)
    get_filename_component(OpenCV_CORE_LIB_DIR "${OpenCV_CORE_DLL_PATH}" DIRECTORY)
    message(STATUS "    DLL might in: ${OpenCV_CORE_LIB_DIR}, OpenCV_CORE_DLL_PATH: ${OpenCV_CORE_DLL_PATH}")
else()
    # 可能是静态库，或者属性未设置
    get_target_property(OpenCV_CORE_LIB_PATH OpenCV::core IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE)
    if(OpenCV_CORE_LIB_PATH)
            get_filename_component(OpenCV_CORE_LIB_DIR "${OpenCV_CORE_LIB_PATH}" DIRECTORY)
            message(STATUS "    .lib is in: ${OpenCV_CORE_LIB_DIR}, OpenCV_CORE_LIB_PATH: ${OpenCV_CORE_LIB_PATH}")
    endif()
endif()


set(libwebp_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/libwebp)


#include_directories(${CMAKE_CURRENT_BINARY_DIR})
option(USE_SYSTEM_WEBP "build with system libwebp" OFF)
if (NOT USE_SYSTEM_WEBP)
    # build libwebp library
    if (NOT EXISTS "${libwebp_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Not find libwebp CMakeLists.txt!")
    endif ()

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

    add_subdirectory(${libwebp_DIR} ${CMAKE_BINARY_DIR}/libwebp)
    include_directories(${CMAKE_BINARY_DIR}/libwebp/src)
endif ()


add_executable(${PROJECT_NAME} main.cpp mnnsr.cpp dcp.cpp)


if (MSVC)  # Visual Studio
    target_link_libraries(${PROJECT_NAME} webp ${OpenCV_LIBS} ${NCNN_LIB} ${MNN_LIB})
    set(TARGET_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/MNNSR-Win64")

    # DLL 路径
    set(MNN_DLL "${mnn_DIR}/lib/${TARGET_ARCH}/Release/Dynamic/MT/MNN.dll")
    set(NCNN_DLL "${ncnn_DIR}/${TARGET_ARCH}/bin/ncnn.dll")

    # 自动查找 OpenCV DLL
    file(GLOB OpenCV_DLLS "${OpenCV_CORE_LIB_DIR}/opencv_world*[0-9].dll")
    message(STATUS "OpenCV_DLLS: ${OpenCV_DLLS}")

    # 合并所有需要复制的 DLL
    set(ALL_DLLS "${MNN_DLL}" "${NCNN_DLL}")
    list(APPEND ALL_DLLS ${OpenCV_DLLS})

    # 生成所有 DLL 的复制命令
    set(DLL_COPY_COMMANDS "")
    foreach(dll ${ALL_DLLS})
        list(APPEND DLL_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "${TARGET_BIN_DIR}/"
        )
    endforeach()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${TARGET_BIN_DIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>
        ${DLL_COPY_COMMANDS}
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} webp ${OpenCV_LIBS} ${NCNN_LIB} ${MNN_LIB})

else ()
    target_link_libraries(${PROJECT_NAME} webp MNN MNN_CL MNN_Vulkan ${OpenCV_LIBS} ${NCNN_LIB})
    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../RealSR-NCNN-Android-GUI/app/src/main/assets/realsr
    )
endif ()

