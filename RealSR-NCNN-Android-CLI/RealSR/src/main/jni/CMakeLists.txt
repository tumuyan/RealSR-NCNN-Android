cmake_minimum_required(VERSION 3.5)
project(realsr-ncnn)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/arch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/libwebp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/opencv.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/ncnn.cmake)

add_executable(${PROJECT_NAME} main.cpp realsr.cpp)

target_link_libraries(${PROJECT_NAME}  webp ${NCNN_LIB} ${OpenCV_LIBS})

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../RealSR-NCNN-Android-GUI/app/src/main/assets/realsr
)

# 获取 Android 构建根目录
get_property(ANDROID_BINARY_DIR GLOBAL PROPERTY ANDROID_BINARY_DIR)

message(CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR})

message(ANDROID_BINARY_DIR: ${ANDROID_BINARY_DIR})

