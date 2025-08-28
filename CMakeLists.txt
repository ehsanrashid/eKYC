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

    if(IWYU_PATH)
        message(STATUS "Found IWYU: ${IWYU_PATH}")

        # Ensure compile_commands.json is written
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

        # Define a custom target "run-iwyu"
        add_custom_target(run-iwyu
            COMMAND python3 /usr/share/include-what-you-use/iwyu_tool.py
                    -p ${CMAKE_BINARY_DIR} -j 4
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running include-what-you-use analysis..."
        )
    else()
        message(WARNING "WITH_IWYU=ON but IWYU not found!")
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
