cmake_minimum_required(VERSION 3.5)
project(resize-ncnn)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/arch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/libwebp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/opencv.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/ncnn.cmake)


add_executable(${PROJECT_NAME} main.cpp)

if (WIN32)
    target_link_libraries(${PROJECT_NAME}  webp ${NCNN_LIB})
else ()
    target_link_libraries(${PROJECT_NAME}  webp ${NCNN_LIB} ${OpenCV_LIBS})
    add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../RealSR-NCNN-Android-GUI/app/src/main/assets/realsr
    )
endif ()

