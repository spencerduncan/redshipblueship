/**
 * @file rsbs_version.c
 * @brief The version string a person sees (#880). See rsbs_version.h.
 */

#include "rsbs_version.h"

/* "+<short commit>" and "-dirty" when tracked files differed from that commit,
 * or "" for a tree without git: computed at configure time in CMakeLists.txt
 * and set on this one source file (CMake/SingleExecutable.cmake), so a new
 * commit recompiles this file alone. */
#ifndef RSBS_BUILD_SUFFIX
#define RSBS_BUILD_SUFFIX ""
#endif

static const char sRsbsDisplay[] = RSBS_VERSION_STRING RSBS_BUILD_SUFFIX;
static const char sRsbsWindowTitle[] =
    RSBS_APP_NAME " " RSBS_VERSION_STRING RSBS_BUILD_SUFFIX " (" RSBS_VERSION_CODENAME ")";

const char* RsbsVersion_Display(void) {
    return sRsbsDisplay;
}

const char* RsbsVersion_WindowTitle(void) {
    return sRsbsWindowTitle;
}
