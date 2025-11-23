
if (MSVC)  # Visual Studio
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-windows-vs2022-shared)
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/${TARGET_ARCH}/lib)
    #  set(ncnn_LIB ${ncnn_DIR}/${TARGET_ARCH}/lib/ncnn.lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(NCNN_LIB ncnn HINTS ${ncnn_DIR}/lib)
    #  set(ncnn_LIB ${ncnn_DIR}/${TARGET_ARCH}/lib/ncnn.lib)
else ()  # Android Studio
    set(ncnn_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/ncnn-android-vulkan-shared)
    #    set(ncnn_LIB ${ncnn_DIR}/${ANDROID_ABI}/lib/libncnn.so)
endif ()
#set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION
#        ${ncnn_DIR}/${ANDROID_ABI}/lib/libncnn.so)

add_library(ncnn SHARED IMPORTED)
include_directories(${ncnn_DIR}/${TARGET_ARCH}/include/ncnn)
# set_target_properties(ncnn PROPERTIES IMPORTED_LOCATION  ${ncnn_LIB})


message(STATUS "Find ncnn in: ${ncnn_DIR}/${TARGET_ARCH}/lib")
find_library(NCNN_LIB NAMES ncnn libncnn PATHS "${ncnn_DIR}/${TARGET_ARCH}/lib")
if (EXISTS ${NCNN_LIB})
    #message(STATUS "Found ncnn: ${NCNN_LIB}")
else ()
    message(STATUS "    ncnn library not found!")
    set(NCNN_LIB "${ncnn_DIR}/${TARGET_ARCH}/lib/libncnn.so")
endif ()
message(STATUS "Found ncnn: ${NCNN_LIB}")
