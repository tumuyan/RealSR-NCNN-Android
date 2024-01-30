target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE
        ${AviSynthPlus_SDK_PATH}/include
)

if(OS_64_Bit)
    find_library(AviSynthPlus_LIBS 
    NAMES AviSynth 
    PATHS ${AviSynthPlus_SDK_PATH}/lib/x64 ${AviSynthPlus_SDK_PATH}/lib64
    NO_DEFAULT_PATH REQUIRED)
else()
    find_library(AviSynthPlus_LIBS 
    NAMES AviSynth 
    PATHS ${AviSynthPlus_SDK_PATH}/lib/x86 ${AviSynthPlus_SDK_PATH}/lib32
    NO_DEFAULT_PATH REQUIRED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Anime4KCPPCore ${AviSynthPlus_LIBS})
