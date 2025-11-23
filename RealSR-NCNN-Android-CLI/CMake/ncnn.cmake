if (MSVC)  # Visual Studio
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-windows-vs2022-shared)
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/${TARGET_ARCH}/lib)
    include_directories(${ncnn_DIR}/${TARGET_ARCH}/include/ncnn)
#    find_library(NCNN_LIB NAMES ncnn libncnn PATHS "${ncnn_DIR}/${TARGET_ARCH}/lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-ubuntu-vulkan-shared)
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/lib)
    include_directories(${ncnn_DIR}/include/ncnn)
#    find_library(NCNN_LIB NAMES ncnn libncnn PATHS "${ncnn_DIR}/lib")
else ()  # Android Studio
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-android-vulkan-shared)
    include_directories(${ncnn_DIR}/${TARGET_ARCH}/include/ncnn)
    find_library(NCNN_LIB NAMES ncnn libncnn PATHS "${ncnn_DIR}/${TARGET_ARCH}/lib")
endif ()


add_library(ncnn SHARED IMPORTED)

message(STATUS "Find ncnn in: ${ncnn_DIR}/")

if (EXISTS ${NCNN_LIB})
    #message(STATUS "Found ncnn: ${NCNN_LIB}")
else ()
    message(STATUS "    ncnn library not found!")
    set(NCNN_LIB "${ncnn_DIR}/${TARGET_ARCH}/lib/libncnn.so")
endif ()
message(STATUS "Found ncnn: ${NCNN_LIB}")
