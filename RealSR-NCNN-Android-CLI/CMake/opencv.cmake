
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../common)
