cmake_minimum_required(VERSION 3.16)
project(eKYC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch Aeron from GitHub
FetchContent_Declare(
    aeron
    GIT_REPOSITORY https://github.com/aeron-io/aeron.git
    GIT_TAG        master  # or specify a specific version tag like "1.45.0"
)

# Configure Aeron build options
set(BUILD_AERON_DRIVER ON CACHE BOOL "Build Aeron driver")
set(BUILD_AERON_ARCHIVE_API ON CACHE BOOL "Build Aeron archive API")
set(C_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable warnings as errors")
set(CXX_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable warnings as errors")

FetchContent_MakeAvailable(aeron)

# Collect source files
file(GLOB SOURCE_FILES "*.cpp" )

# Create a single eKYC executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Include Aeron headers and project includes
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${aeron_SOURCE_DIR}/aeron-client/src/main/cpp
    ${aeron_SOURCE_DIR}/aeron-driver/src/main/c
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/output/my_app_messages
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/libaeronWrapper.a
    aeron_driver
    aeron_client
    /usr/local/lib/libloggerlib.a
)
