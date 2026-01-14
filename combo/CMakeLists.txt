cmake_minimum_required(VERSION 3.14)

add_library(combo STATIC
    src/placeholder.cpp
)

target_include_directories(combo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(combo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
