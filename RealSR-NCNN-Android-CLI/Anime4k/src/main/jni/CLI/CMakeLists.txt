if(Build_CLI)
    project(Anime4k LANGUAGES CXX)

    if(NOT Enable_Image_IO)
        message (
            FATAL_ERROR "To compile the CLI, you cannot turn off image IO support from the core\n"
        )
    endif()

    aux_source_directory(src SOURCE)
    
    file(GLOB INCLUDE include/*.hpp)

    add_executable(${PROJECT_NAME} ${INCLUDE} ${SOURCE})

    target_include_directories(${PROJECT_NAME} PRIVATE include)

    target_compile_definitions(
        ${PROJECT_NAME} 
        PRIVATE 
            $<$<BOOL:${Use_Boost_filesystem}>:USE_BOOST_FILESYSTEM>
    )

    if(Other_Optimization_For_Other)
        if(MSVC)
            if(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
                target_compile_options(${PROJECT_NAME} PRIVATE /O3)
            endif()
        else()
            target_compile_options(${PROJECT_NAME} PRIVATE -Ofast)
            target_link_options(${PROJECT_NAME} PRIVATE -Ofast)
        endif()
    endif()

    include(${TOP_DIR}/cmake/ThirdPartyForCLI.cmake)

    install(
        TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )

endif()
