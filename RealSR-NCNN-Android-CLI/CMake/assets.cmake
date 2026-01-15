if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # 1. 先手动创建目标文件夹
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ASSETS_DIR}
        
        # 2. 只复制 .exe 文件 (使用 copy 而不是 copy_directory)
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>  
        ${ASSETS_DIR}/
        
        # 3. 额外 copy DLL
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        ${ASSETS_DIR}/

        # 4. 复制 ncnn.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ncnn_DIR}/${TARGET_ARCH}/bin/ncnn.dll"
        ${ASSETS_DIR}/

        COMMAND_EXPAND_LISTS
    )
else()
    # --- Linux / Android 逻辑 ---
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        ${ASSETS_DIR}
    )
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    # 获取 Android 构建根目录
    get_property(ANDROID_BINARY_DIR GLOBAL PROPERTY ANDROID_BINARY_DIR)
    message(CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR})
    message(ANDROID_BINARY_DIR: ${ANDROID_BINARY_DIR})
endif()