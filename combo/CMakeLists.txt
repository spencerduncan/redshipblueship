cmake_minimum_required(VERSION 3.14)

add_library(combo STATIC
    src/placeholder.cpp
    src/GameArchives.cpp
    src/DynamicLibrary.cpp
)

target_include_directories(combo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against libultraship for Archive/File types
target_link_libraries(combo PUBLIC
    libultraship
)

# Link against dl for dynamic loading on Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(combo PUBLIC ${CMAKE_DL_LIBS})
endif()

set_target_properties(combo PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
