
if (MSVC)  # Visual Studio
    set(OpenCV_DIR "C:/Lib/opencv/build/x64/vc16/lib")
    # set(OpenCV_DIR "C:/Lib/opencv/build")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "Opencv not configured for linux")
else ()  # Android Studio
    set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../3rdparty/OpenCV-android-sdk/sdk/native/jni/)
endif ()



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
