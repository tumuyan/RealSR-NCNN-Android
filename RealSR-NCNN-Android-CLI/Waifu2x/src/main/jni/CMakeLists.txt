cmake_minimum_required(VERSION 3.5)
project(waifu2x-ncnn)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/arch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/libwebp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/opencv.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/ncnn.cmake)



add_executable(${PROJECT_NAME}  main.cpp waifu2x.cpp)

target_link_libraries(${PROJECT_NAME} webp ${NCNN_LIB} ${OpenCV_LIBS})

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/assets.cmake)
