cmake_minimum_required(VERSION 3.5)
project(mnnsr-ncnn)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/arch.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/libwebp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/opencv.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../CMake/mnn.cmake)

add_executable(${PROJECT_NAME} main.cpp mnnsr.cpp dcp.cpp)


if (MSVC)  # Visual Studio
    target_link_libraries(${PROJECT_NAME} webp ${OpenCV_LIBS} ${MNN_LIB})
    set(TARGET_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/MNNSR-Win64")

    # DLL 路径
    set(MNN_DLL "${mnn_DIR}/lib/${TARGET_ARCH}/Release/Dynamic/MT/MNN.dll")


    # 自动查找 OpenCV DLL
    file(GLOB OpenCV_DLLS "${OpenCV_CORE_LIB_DIR}/opencv_world*[0-9].dll")
    # message(STATUS "OpenCV_DLLS: ${OpenCV_DLLS}")

    # 合并所有需要复制的 DLL
    set(ALL_DLLS "${MNN_DLL}")
    list(APPEND ALL_DLLS ${OpenCV_DLLS})
    
    message(STATUS "ALL_DLLS: ${ALL_DLLS}")

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
    target_link_libraries(${PROJECT_NAME} webp ${OpenCV_LIBS} ${MNN_LIB})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN/lib"
    )

else ()
    target_link_libraries(${PROJECT_NAME} webp MNN MNN_CL MNN_Vulkan ${OpenCV_LIBS})
    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../RealSR-NCNN-Android-GUI/app/src/main/assets/realsr
    )
endif ()

