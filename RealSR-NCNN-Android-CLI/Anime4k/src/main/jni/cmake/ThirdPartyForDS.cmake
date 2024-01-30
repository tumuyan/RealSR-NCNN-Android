target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE
        ${DirectShow_SDK_PATH}
)

if(OS_64_Bit)
    find_library(DirectShow_LIBS 
    NAMES strmbase 
    PATHS ${DirectShow_SDK_PATH}/x64/Release 
    NO_DEFAULT_PATH REQUIRED)
else()
    find_library(DirectShow_LIBS 
    NAMES strmbase
    PATHS ${DirectShow_SDK_PATH}/Release 
    NO_DEFAULT_PATH REQUIRED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Anime4KCPPCore ${DirectShow_LIBS} winmm)
