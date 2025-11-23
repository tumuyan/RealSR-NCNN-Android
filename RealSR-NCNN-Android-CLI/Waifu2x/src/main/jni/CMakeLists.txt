cmake_minimum_required(VERSION 3.5)
project(waifu2x-ncnn)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/arch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/libwebp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/opencv.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/ncnn.cmake)


add_executable(${PROJECT_NAME}  main.cpp waifu2x.cpp)

target_link_libraries(${PROJECT_NAME} webp ${NCNN_LIB} ${OpenCV_LIBS})

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../RealSR-NCNN-Android-GUI/app/src/main/assets/realsr
)