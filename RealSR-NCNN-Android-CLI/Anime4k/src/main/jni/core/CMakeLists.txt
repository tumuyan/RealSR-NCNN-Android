project(Anime4KCPPCore VERSION 2.6.0.4 LANGUAGES CXX)

aux_source_directory(src SOURCE)

file(GLOB INCLUDE include/*.hpp)

if(Build_Static_Core)
    add_library(${PROJECT_NAME} STATIC 
        ${INCLUDE} ${SOURCE} 
        $<$<BOOL:${Enable_CUDA}>:$<TARGET_OBJECTS:CUDA_Module>>
    )
else()
    add_library(${PROJECT_NAME} SHARED 
        ${INCLUDE} ${SOURCE} 
        $<$<BOOL:${Enable_CUDA}>:$<TARGET_OBJECTS:CUDA_Module>>
    )
endif()

target_compile_definitions(
    ${PROJECT_NAME} 
    PUBLIC 
        $<$<BOOL:${Enable_OpenCL}>:ENABLE_OPENCL>
        $<$<BOOL:${Enable_CUDA}>:ENABLE_CUDA>
        $<$<BOOL:${Enable_NCNN}>:ENABLE_NCNN>
        $<$<BOOL:${Enable_Video}>:ENABLE_VIDEO>
        $<$<BOOL:${Enable_Preview_GUI}>:ENABLE_PREVIEW_GUI>
        $<$<BOOL:${Enable_Image_IO}>:ENABLE_IMAGE_IO>

        ANIME4KCPP_CORE_PARALLEL_LIBRARY="${Parallel_Library_Type}"
        ANIME4KCPP_CORE_COMPILER="${CMAKE_CXX_COMPILER_ID}" 
        ANIME4KCPP_CORE_BUILD_DATE="${TODAY}"

        ANIME4KCPP_CORE_VERSION="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
        ANIME4KCPP_CORE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        ANIME4KCPP_CORE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        ANIME4KCPP_CORE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        ANIME4KCPP_CORE_VERSION_STATUS=$<IF:$<VERSION_LESS:${PROJECT_VERSION_TWEAK},5>,"dev","stb">
    PRIVATE
        $<$<BOOL:${Built_In_Kernel}>:BUILT_IN_KERNEL>
        $<$<BOOL:${Enable_OpenCV_DNN}>:ENABLE_OPENCV_DNN>
        $<$<BOOL:${Ryzen_Optimization}>:USE_RYZEN>
        $<$<BOOL:${Use_Eigen3}>:USE_EIGEN3>
        $<$<BOOL:${Use_Legacy_OpenCL_API}>:LEGACY_OPENCL_API>
        $<$<BOOL:${Enable_Fast_Math}>:ENABLE_FAST_MATH>
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    PUBLIC_HEADER "${INCLUDE};${CMAKE_CURRENT_BINARY_DIR}/ac_export.h"
)

if(Build_Static_Core)
    set_target_properties(
        ${PROJECT_NAME} PROPERTIES 
            POSITION_INDEPENDENT_CODE ${Build_Static_Core_PIC}
    )
endif()

if(Other_Optimization_For_Core)
    if(MSVC)
        if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
            target_compile_options(${PROJECT_NAME} PRIVATE /O3)
        endif()
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Ofast)
        target_link_options(${PROJECT_NAME} PRIVATE -Ofast)
    endif()
endif()

include(${TOP_DIR}/cmake/ThirdPartyForCore.cmake)

install(
    TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
    ARCHIVE DESTINATION core/lib
    LIBRARY DESTINATION core/lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION core/include
)

install(DIRECTORY models/ DESTINATION bin/models)
install(DIRECTORY kernels/ DESTINATION bin/kernels)

install(
    EXPORT ${PROJECT_NAME}
    NAMESPACE Anime4KCPP::
    DESTINATION core/cmake
)

configure_file(${TOP_DIR}/cmake/Anime4KCPPCoreConfig.cmake.in Anime4KCPPCoreConfig.cmake @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Anime4KCPPCoreConfig.cmake DESTINATION core/cmake)
