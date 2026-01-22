cmake_minimum_required(VERSION 3.16)

# rsbs - RedShipBlueShip unified game engine
# Contains code shared between OoT and MM
#
# This library will grow as identical code is migrated from both games.
# The goal is ONE unified game engine, not two games that switch.

add_library(rsbs STATIC
    # Files will be added as code is migrated
    src/placeholder.c
)

target_include_directories(rsbs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against libultraship for common types
target_link_libraries(rsbs PUBLIC
    libultraship
)

# Suppress warnings for migrated decomp code
if(SUPPRESS_WARNINGS)
    target_compile_options(rsbs PRIVATE "${WARNING_OVERRIDE}")
endif()

set_target_properties(rsbs PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
)
