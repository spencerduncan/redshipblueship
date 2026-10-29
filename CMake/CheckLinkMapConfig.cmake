# CMake/CheckLinkMapConfig.cmake
#
# #1303 lock: release crash tracebacks print each frame as redship.exe+0xOFFSET
# (#1199), which resolves against redship.map. Verify that:
#   1. RSBS_EMIT_LINK_MAP is ON by default and the configured `redship` target's
#      LINK_OPTIONS carry /MAP (Windows/MSVC) or -Wl,-Map=<build>/redship.map
#      (Linux), rather than relying on a workstation-local CMAKE_EXE_LINKER_FLAGS
#      cache entry;
#   2. ${BINARY_DIR}/redship.map exists and names Context_GetCurrentGame;
#   3. CMakeLists.txt and CMake/SingleExecutable.cmake do not install redship.map
#      into any CPack component (keeping it out of the player zip);
#   4. .github/workflows/generate-builds.yml verifies redship.map and
#      Context_GetCurrentGame on both legs, uploads redship-map-windows and
#      redship-map-linux as separate artifacts beside rsbs-windows / rsbs-linux,
#      and attaches redshipblueship-<tag>-windows.map and
#      redshipblueship-<tag>-linux.map to the GitHub Release.
#
#   cmake -DSOURCE_DIR=<source dir> -DBINARY_DIR=<build dir>
#         -DEMIT_LINK_MAP=<ON|OFF> -DRED_LINK_OPTIONS=<;-or-|-joined options>
#         -P CheckLinkMapConfig.cmake

foreach(_required SOURCE_DIR BINARY_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "CheckLinkMapConfig: -D${_required}=<dir> is required")
    endif()
endforeach()

set(_failures 0)
function(_fail _msg)
    message(SEND_ERROR "CheckLinkMapConfig: FAIL ${_msg}")
    math(EXPR _n "${_failures} + 1")
    set(_failures ${_n} PARENT_SCOPE)
endfunction()

# 1. Configured option + target LINK_OPTIONS on `redship`
if(NOT EMIT_LINK_MAP)
    _fail("RSBS_EMIT_LINK_MAP is '${EMIT_LINK_MAP}' (expected ON by default)")
else()
    message(STATUS "CheckLinkMapConfig: RSBS_EMIT_LINK_MAP='${EMIT_LINK_MAP}' ok")
endif()

if(CMAKE_HOST_WIN32)
    if(NOT RED_LINK_OPTIONS MATCHES "(^|[|;])/MAP([|;:]|$)")
        _fail("redship LINK_OPTIONS '${RED_LINK_OPTIONS}' does not carry /MAP on Windows")
    else()
        message(STATUS "CheckLinkMapConfig: redship LINK_OPTIONS carries /MAP ok")
    endif()
elseif(CMAKE_HOST_UNIX AND NOT CMAKE_HOST_APPLE)
    if(NOT RED_LINK_OPTIONS MATCHES "-Wl,-Map=.*redship\\.map")
        _fail("redship LINK_OPTIONS '${RED_LINK_OPTIONS}' does not carry -Wl,-Map=.../redship.map on Linux")
    else()
        message(STATUS "CheckLinkMapConfig: redship LINK_OPTIONS carries -Wl,-Map=.../redship.map ok")
    endif()
endif()

# 2. Live redship.map in the build directory names Context_GetCurrentGame
#    (src/common/context.cpp, referenced on every platform; a symbol nothing on
#    Linux calls, such as Combo_SetSharedGraphics, is never pulled out of its
#    static archive by GNU ld and so never appears in the Linux map).
set(_map_file "${BINARY_DIR}/redship.map")
if(NOT EXISTS "${_map_file}")
    _fail("expected linker map '${_map_file}' does not exist")
else()
    file(STRINGS "${_map_file}" _symbol_hits REGEX "Context_GetCurrentGame" LIMIT_COUNT 1)
    if(NOT _symbol_hits)
        _fail("'${_map_file}' does not name Context_GetCurrentGame")
    else()
        message(STATUS "CheckLinkMapConfig: '${_map_file}' names Context_GetCurrentGame ok")
    endif()
endif()

# 3. CMake source files configure RSBS_EMIT_LINK_MAP and never install redship.map
set(_single_exe_cmake "${SOURCE_DIR}/CMake/SingleExecutable.cmake")
set(_root_cmake "${SOURCE_DIR}/CMakeLists.txt")
if(NOT EXISTS "${_single_exe_cmake}" OR NOT EXISTS "${_root_cmake}")
    _fail("source CMake files not found under '${SOURCE_DIR}'")
else()
    file(READ "${_single_exe_cmake}" _se_raw)
    file(READ "${_root_cmake}" _root_raw)
    string(REGEX REPLACE "#[^\n]*" "" _se_code "${_se_raw}")
    string(REGEX REPLACE "#[^\n]*" "" _root_code "${_root_raw}")

    if(NOT _se_code MATCHES "option\\([ \t\r\n]*RSBS_EMIT_LINK_MAP[^)]*[ \t\r\n]ON[ \t\r\n]*\\)"
       AND NOT _root_code MATCHES "option\\([ \t\r\n]*RSBS_EMIT_LINK_MAP[^)]*[ \t\r\n]ON[ \t\r\n]*\\)")
        _fail("neither CMakeLists.txt nor CMake/SingleExecutable.cmake declares option(RSBS_EMIT_LINK_MAP ... ON)")
    endif()
    if(NOT _se_code MATCHES "target_link_options\\([ \t\r\n]*redship[ \t\r\n]+PRIVATE[ \t\r\n]+/MAP")
        _fail("CMake/SingleExecutable.cmake does not set target_link_options(redship PRIVATE /MAP) for MSVC")
    endif()
    if(NOT _se_code MATCHES "target_link_options\\([ \t\r\n]*redship[ \t\r\n]+PRIVATE[ \t\r\n]+\"-Wl,-Map=\\$\\{CMAKE_BINARY_DIR\\}/redship\\.map\"\\)")
        _fail("CMake/SingleExecutable.cmake does not set target_link_options(redship PRIVATE \"-Wl,-Map=\${CMAKE_BINARY_DIR}/redship.map\") for Linux")
    endif()
    if(_se_code MATCHES "install\\([^)]*redship\\.map" OR _root_code MATCHES "install\\([^)]*redship\\.map")
        _fail("redship.map must not be passed to install() (it must stay out of the CPack player zip)")
    endif()
endif()

# 4. .github/workflows/generate-builds.yml verifies, uploads and attaches the map
set(_workflow "${SOURCE_DIR}/.github/workflows/generate-builds.yml")
if(NOT EXISTS "${_workflow}")
    _fail("workflow '${_workflow}' not found")
else()
    file(READ "${_workflow}" _wf_raw)
    string(REGEX REPLACE "#[^\n]*" "" _wf_code "${_wf_raw}")

    foreach(_needle
            "build-cmake/redship.map"
            "bw/redship.map"
            "Context_GetCurrentGame"
            "name: redship-map-linux"
            "name: redship-map-windows"
            "redshipblueship-\${RELEASE_TAG}-linux.map"
            "redshipblueship-\${RELEASE_TAG}-windows.map")
        string(FIND "${_wf_code}" "${_needle}" _pos)
        if(_pos EQUAL -1)
            _fail(".github/workflows/generate-builds.yml is missing '${_needle}'")
        endif()
    endforeach()
endif()

if(_failures GREATER 0)
    message(FATAL_ERROR "CheckLinkMapConfig: ${_failures} failure(s)")
endif()
message(STATUS "CheckLinkMapConfig: PASS")
