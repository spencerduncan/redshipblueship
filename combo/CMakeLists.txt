cmake_minimum_required(VERSION 3.14)

add_library(combo STATIC
    src/placeholder.cpp
    src/GameArchives.cpp
    src/DynamicLibrary.cpp
    src/ComboContextBridge.cpp
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

# Launcher executable (only when building shared libraries)
if(REDSHIP_BUILD_SHARED)
    add_executable(redship
        src/main.cpp
    )

    target_link_libraries(redship PRIVATE
        combo
    )

    set_target_properties(redship PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Install the launcher
    install(TARGETS redship RUNTIME DESTINATION bin)
endif()
