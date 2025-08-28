cmake_minimum_required(VERSION 3.16)
project(eKYC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the wrapper package (installed to /usr/local or a custom prefix)
find_package(aeronWrapper CONFIG REQUIRED)

# Find libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

option(WITH_IWYU "Run include-what-you-use analysis" OFF)

if(WITH_IWYU)
    find_program(IWYU_PATH NAMES include-what-you-use iwyu)

    set(IWYU_TOOL_PATH "/usr/share/include-what-you-use/iwyu_tool.py")
    if(NOT EXISTS ${IWYU_TOOL_PATH})
        set(IWYU_TOOL_PATH "${CMAKE_SOURCE_DIR}/iwyu_tool.py")
    endif()

    if(IWYU_PATH AND EXISTS ${IWYU_TOOL_PATH})
        message(STATUS "Found IWYU: ${IWYU_PATH}")
        message(STATUS "Using IWYU tool: ${IWYU_TOOL_PATH}")
        add_custom_target(run-iwyu
            COMMAND python3 ${IWYU_TOOL_PATH} -p ${CMAKE_BINARY_DIR} -j 4
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running include-what-you-use analysis..."
        )
    else()
        message(WARNING "IWYU or iwyu_tool.py not found, skipping run-iwyu target")
    endif()
endif()

# Collect source files
file(GLOB SOURCES "src/*.cpp")

# Create a single executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include project includes and lib includes
target_include_directories(${PROJECT_NAME} PRIVATE
    /usr/local/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    aeronWrapper::aeronWrapper
    /usr/local/lib/libquillLogger.a
    /usr/local/lib/libDbFactory.a
    ${PQXX_LIBRARIES}
)
