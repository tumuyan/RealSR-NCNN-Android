
if (MSVC)  # Visual Studio

    # 设置目标属性，指定 MNN 库的路径
    set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_windows_x64_cpu_opencl)
    # set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_windows)
    find_library(MNN_LIB mnn HINTS "${mnn_DIR}/lib/${TARGET_ARCH}/Release/Dynamic/MT")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/MNN)
    find_library(MNN_LIB MNN HINTS "${mnn_DIR}/libs")

    # set(mnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/mnn_linux_x64)
    # find_library(MNN_LIB MNN HINTS "${mnn_DIR}/lib/Release")
    # include_directories(${mnn_DIR}/include)
    message(STATUS "find mnn dir: ${mnn_DIR}")
    message(STATUS "find mnn: ${MNN_LIB}")
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-ubuntu-shared)
else()

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
endif ()


include_directories(${mnn_DIR}/include)
#include_directories(${mnn_DIR}/${ANDROID_ABI}/include/MNN)

message(STATUS "Find mnn in: ${mnn_DIR}")
if (EXISTS ${MNN_LIB})
    #message(STATUS "find mnn: ${MNN_LIB}")
else ()
    message(STATUS "    mnn library not found!")
    set(MNN_LIB "${mnn_DIR}/${ANDROID_ABI}/libMNN.so")
endif ()
message(STATUS "Found mnn: ${MNN_LIB}")