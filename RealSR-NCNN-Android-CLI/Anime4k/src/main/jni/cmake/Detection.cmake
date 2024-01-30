set(TMP_DIR ${PROJECT_BINARY_DIR}/tmp)
set(DetectionSample_DIR ${TOP_DIR}/cmake/DetectionSample)

set(has_filesystem_cpp
"#include <filesystem>

int main()
{
    std::filesystem::path hasFS(\"./\")\;
    return 0\;
}
")

if(NOT EXISTS ${DetectionSample_DIR}/has_filesystem.cpp)
    file(MAKE_DIRECTORY ${DetectionSample_DIR})
    file(WRITE ${DetectionSample_DIR}/has_filesystem.cpp ${has_filesystem_cpp})
endif()

# check std::filesystem
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    try_compile(HAS_FILESYSTEM
        ${TMP_DIR}
        ${DetectionSample_DIR}/has_filesystem.cpp
        CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DLINK_LIBRARIES=stdc++fs
        OUTPUT_VARIABLE HAS_FILESYSTEM_MSG
    )
else()
    try_compile(HAS_FILESYSTEM
        ${TMP_DIR}
        ${DetectionSample_DIR}/has_filesystem.cpp
        CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
        OUTPUT_VARIABLE HAS_FILESYSTEM_MSG
    )
endif()
