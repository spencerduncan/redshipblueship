cmake_minimum_required(VERSION 3.14)

# rsbs - N64 ABI compatibility shims (libultra/os, libultra/gu)
# Contains libultra OS and graphics-utility functions that are identical
# between OoT and MM and shared across both.
#
# This is NOT the unified host-side game layer. Game lifecycle, cross-game
# state, entrance switching, save unification, etc. live in src/common/.
# See docs/adr/0001-rsbs-vs-src-common.md for the directory-role decision.

add_library(rsbs STATIC
    # Core shared utilities
    src/placeholder.c

    # libultra/os - Unified OS functions (WP-009)
    src/libultra/os/afterprenmi.c
    src/libultra/os/getactivequeue.c
    src/libultra/os/getthreadid.c
    src/libultra/os/getthreadpri.c
    src/libultra/os/destroythread.c
    src/libultra/os/dequeuethread.c

    # libultra/os - message queue functions (reference implementations)
    # Note: Runtime implementations provided by libultraship
    # src/libultra/os/createmesgqueue.c
    # src/libultra/os/sendmesg.c
    # src/libultra/os/recvmesg.c
    # src/libultra/os/jammesg.c
    # src/libultra/os/seteventmesg.c

    # libultra/gu - Graphics utility functions
    # sqrtf.c removed - caused infinite recursion when __builtin_sqrtf emits sqrtf call
    src/libultra/gu/position.c
    src/libultra/gu/rotate.c
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

# Use C11 for broad compiler compatibility (MSVC doesn't support C23)
set_target_properties(rsbs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
