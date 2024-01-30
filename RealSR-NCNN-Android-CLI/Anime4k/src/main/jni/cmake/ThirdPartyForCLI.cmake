set(CMDLINE_HPP_URL https://github.com/TianZerL/cmdline/raw/095f7129afb7be56e918948076567731a606e1e8/cmdline.hpp)
set(SHA1_CMDLINE "c2d8d368a097feb6da1ec6d50735b13f2d082388")

set(INI17_HPP_URL https://github.com/TianZerL/ini17/raw/0e6a79fba398e4b9d6312f00bdd52920d1be3c23/src/ini17.hpp)
set(SHA1_INI17 "82a581102e54dac85a9ef2c7d4b9042428d5240a")

if(EXISTS ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp)
    file(SHA1 ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp LOCAL_SHA1_CMDLINE)

    if(NOT ${LOCAL_SHA1_CMDLINE} STREQUAL ${SHA1_CMDLINE})
        message("Warning:")
        message("   Local SHA1 for comline.hpp:   ${LOCAL_SHA1_CMDLINE}")
        message("   Expected SHA1:              ${SHA1_CMDLINE}")
        message("   Mismatch SHA1 for cmdline.hpp, trying to download it...")

        file(
            DOWNLOAD ${CMDLINE_HPP_URL} ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp 
            SHOW_PROGRESS 
            EXPECTED_HASH SHA1=${SHA1_CMDLINE}
        )

    endif()
else()
    file(
        DOWNLOAD ${CMDLINE_HPP_URL} ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp 
        SHOW_PROGRESS 
        EXPECTED_HASH SHA1=${SHA1_CMDLINE}
    )
endif()

if(EXISTS ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp)
    file(SHA1 ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp LOCAL_SHA1_INI17)

    if(NOT ${LOCAL_SHA1_INI17} STREQUAL ${SHA1_INI17})
        message("Warning:")
        message("   Local SHA1 for ini17.hpp:   ${LOCAL_SHA1_INI17}")
        message("   Expected SHA1:              ${SHA1_INI17}")
        message("   Mismatch SHA1 for ini17.hpp, trying to download it...")

        file(
            DOWNLOAD ${INI17_HPP_URL} ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp
            SHOW_PROGRESS 
            EXPECTED_HASH SHA1=${SHA1_INI17}
        )

    endif()
else()
    file(
        DOWNLOAD ${INI17_HPP_URL} ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp
        SHOW_PROGRESS 
        EXPECTED_HASH SHA1=${SHA1_INI17}
    )
endif()

find_package(CURL)
if(${CURL_FOUND})
    message(STATUS "CLI: libcurl found, enable web image download support.")
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
    add_definitions(-DENABLE_LIBCURL)
endif()

target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE
        ${TOP_DIR}/ThirdParty/include/cmdline
        ${TOP_DIR}/ThirdParty/include/ini17
)

target_link_libraries(${PROJECT_NAME} PRIVATE Anime4KCPPCore)

if(Use_Boost_filesystem)
    find_package(Boost COMPONENTS filesystem REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0) # Just for G++-8 to enable filesystem
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()
