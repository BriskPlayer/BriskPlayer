# BriskPlayer Unit Tests
cmake_minimum_required(VERSION 3.20)

project(briskplayer_tests LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include parent directory for headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Test executable
add_executable(tests
    tests.c
)

# Link with Windows libraries
if(WIN32)
    target_link_libraries(tests PRIVATE
        kernel32
        user32
    )
endif()

# Add test
enable_testing()
add_test(NAME unit_tests COMMAND tests)
