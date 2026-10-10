# CMake/RsbsBuildIdentity.cmake
#
# The "-dirty" half of the build identity a person sees (#880): the window title
# and --version read RSBS_VERSION_STRING plus "+<short commit>", and "-dirty"
# when tracked files differ from that commit at configure time.
#
# One function, used by CMakeLists.txt at configure time and by the
# BuildIdentityDirty row (CMake/CheckBuildIdentityDirty.cmake), which runs it on
# a scratch repository with known edits. That row is the dirty bit's own oracle:
# the version rows take their expected "-dirty" from the configure, so they
# could not see a detection that is always 0 or always 1.
#
# rsbs_git_tree_dirty(<repo dir> <out var>)
#   Sets <out var> to 1 when `git status` reports a change to a tracked file
#   (staged or not, a deletion included, or a submodule whose checked-out commit
#   or tracked content differs), else 0. Untracked files do not count. Nor do
#   the four files the game CMakeLists regenerate on every configure (both
#   build.c and both properties.h): they are tracked, so without the excludes
#   every configured tree would read dirty. A git failure reads clean.

include_guard(GLOBAL)

set(RSBS_BUILD_IDENTITY_REGENERATED_FILES
    "games/oot/src/boot/build.c"
    "games/oot/properties.h"
    "games/mm/src/boot/build.c"
    "games/mm/windows/properties.h"
)

function(rsbs_git_tree_dirty _repo_dir _out_var)
    set(_excludes "")
    foreach(_file IN LISTS RSBS_BUILD_IDENTITY_REGENERATED_FILES)
        list(APPEND _excludes ":(exclude)${_file}")
    endforeach()
    execute_process(
        COMMAND git status --porcelain --untracked-files=no --ignore-submodules=untracked -- . ${_excludes}
        WORKING_DIRECTORY "${_repo_dir}"
        OUTPUT_VARIABLE _status
        RESULT_VARIABLE _result
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_result EQUAL 0 AND NOT _status STREQUAL "")
        set(${_out_var} 1 PARENT_SCOPE)
    else()
        set(${_out_var} 0 PARENT_SCOPE)
    endif()
endfunction()
