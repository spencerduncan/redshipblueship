/**
 * @file rsbs_version.c
 * @brief The version string a person sees (#880). See rsbs_version.h.
 */

#include "rsbs_version.h"

static const char sRsbsDisplay[] = RSBS_VERSION_STRING;
static const char sRsbsWindowTitle[] = RSBS_WINDOW_TITLE;

const char* RsbsVersion_Display(void) {
    return sRsbsDisplay;
}

const char* RsbsVersion_WindowTitle(void) {
    return sRsbsWindowTitle;
}
