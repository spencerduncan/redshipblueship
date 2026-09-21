# Keep ninja's MSVC dependency records honest under a shared sccache (#676).
#
# The hazard, in one paragraph. With the Ninja generator on MSVC, CMake writes
# `deps = msvc` and ninja learns each object's header dependencies by parsing
# cl.exe's `/showIncludes` lines off stdout. cl prints every header the way the
# include search spelled it, and CMake's Ninja generator emits ABSOLUTE `-I`
# flags for every directory outside the build tree, so those lines carry
# absolute paths rooted in the checkout that ran the compile. sccache stores the
# compiler's stdout next to the object and replays it verbatim on a cache hit,
# while deliberately keeping `-I` flags OUT of the cache key (the preprocessed
# output it hashes is produced with `/EP`, which emits no path information at
# all). Two checkouts of this repo that share one sccache therefore hit each
# other's objects, and the second one records the FIRST one's header paths.
# Editing a header in the second checkout then changes a file ninja is not
# watching: the object is never recompiled. It surfaces either as LNK2019 on a
# symbol no source at HEAD mentions, or silently, as stale objects that pass
# tests.
#
# This is not hypothetical on the workstation that gates this repo: its main
# checkout's build-cmake/.ninja_deps carried 5203 dependency records naming
# headers under fifteen different sibling worktrees (2094 under `games/mm`,
# 1336 under `games/oot`, 74 under `src/common`).
#
# The fix: partition the cache by source tree, so an object is only ever served
# to the checkout whose headers its dependency records name. sccache's
# documented `SCCACHE_C_CUSTOM_CACHE_BUSTER` is mixed into the C/C++ cache key,
# and sccache reads it per invocation, so a generated launcher shim can set it
# to this tree's path. Nothing is disabled: every rebuild, branch switch and
# object-dir wipe inside this checkout still hits the cache, and a single-tree
# builder (every CI runner, every ordinary developer clone) sees a constant
# buster value and therefore no change in hit rate at all. What is given up is
# cross-CHECKOUT reuse on a machine that builds several checkouts of this repo
# from one cache, which is exactly the reuse that was producing wrong answers.
#
# Measured alternatives that do NOT work, so nobody re-tries them:
#   * `SCCACHE_DIRECT=false` (disable the preprocessor cache): the replay still
#     happens. Reproduced with the defect intact.
#   * `SCCACHE_BASEDIR=<common ancestor>`: rewrites preprocessor output, not the
#     stored `/showIncludes` stdout. Reproduced with the defect intact.
#   * Relative `-I` flags would fix it for free and keep cross-checkout reuse
#     (verified: relative include flags produce honest deps on a cross-checkout
#     hit), but CMake's Ninja generator has no supported knob to emit them.
#   * A private `SCCACHE_DIR` per checkout is correct but also splits the disk
#     budget and the eviction pool; this option keeps one pool.
#
# See docs/BUILDING_WINDOWS.md for the operator-facing version.

option(RSBS_SCCACHE_TREE_PARTITION
       "Partition the sccache key by source tree so ninja's MSVC /showIncludes dependency records cannot come from another checkout (#676)"
       ON)

function(_rsbs_sccache_tree_partition)
    # Only the MSVC + Ninja + sccache combination has the hazard. GCC/Clang
    # write a real depfile (`-MD`), which ninja reads from disk rather than from
    # the compiler's replayed stdout, so ccache/sccache on those toolchains are
    # unaffected and are left alone.
    if(NOT MSVC)
        return()
    endif()
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        return()
    endif()

    set(_launcher "")
    foreach(_var CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)
        if(${_var})
            list(GET ${_var} 0 _candidate)
            if(_candidate MATCHES "sccache")
                set(_launcher "${_candidate}")
                break()
            endif()
        endif()
    endforeach()
    if(NOT _launcher)
        return()
    endif()

    # Already wrapped by a previous configure in this build tree.
    if(_launcher MATCHES "rsbs-sccache-tree")
        return()
    endif()

    if(NOT RSBS_SCCACHE_TREE_PARTITION)
        message(STATUS
            "sccache: tree partitioning DISABLED (RSBS_SCCACHE_TREE_PARTITION=OFF). "
            "If another checkout of this repo shares this sccache, ninja's dependency "
            "records may name that checkout's headers and header edits may not rebuild (#676).")
        return()
    endif()

    # Resolve the launcher to a real executable: `sccache` off PATH is the
    # common spelling, but an absolute path is also accepted.
    if(IS_ABSOLUTE "${_launcher}" AND EXISTS "${_launcher}")
        set(_sccache "${_launcher}")
    else()
        get_filename_component(_launcher_name "${_launcher}" NAME)
        find_program(RSBS_SCCACHE_EXECUTABLE NAMES "${_launcher_name}" sccache)
        if(NOT RSBS_SCCACHE_EXECUTABLE)
            message(WARNING
                "sccache: launcher '${_launcher}' could not be resolved to an executable, so the "
                "#676 dependency-record guard is not active. Wipe object directories after any "
                "header change if another checkout shares this cache.")
            return()
        endif()
        set(_sccache "${RSBS_SCCACHE_EXECUTABLE}")
    endif()

    # The key is the SOURCE tree, not the build tree: two build directories in
    # one checkout see identical absolute header paths, so they may share
    # objects safely, and keeping them together preserves the most reuse.
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" _tree_key)
    file(TO_NATIVE_PATH "${_sccache}" _sccache_native)
    set(_shim "${CMAKE_BINARY_DIR}/rsbs-sccache-tree.cmd")
    file(CONFIGURE
        OUTPUT "${_shim}"
        CONTENT "@echo off
rem Generated by CMake/SccacheWorktreeDeps.cmake -- do not edit, do not commit.
rem Mixes this source tree's identity into sccache's C/C++ cache key so an
rem object compiled in another checkout is never served here, which is what
rem keeps ninja's /showIncludes dependency records pointing at THIS tree (#676).
set \"SCCACHE_C_CUSTOM_CACHE_BUSTER=@_tree_key@\"
\"@_sccache_native@\" %*
exit /b %ERRORLEVEL%
"
        @ONLY
        NEWLINE_STYLE WIN32)

    # Normal (non-cache) variables: the cache keeps whatever the operator passed
    # on the command line, and every reconfigure re-derives the shim from it.
    # These are set in the top-level scope, before any target is created, so
    # every target created afterwards inherits the shim as its launcher.
    set(CMAKE_C_COMPILER_LAUNCHER "${_shim}" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_shim}" PARENT_SCOPE)
    message(STATUS "sccache: partitioning the C/C++ cache key by source tree (#676): ${_tree_key}")
endfunction()

_rsbs_sccache_tree_partition()
