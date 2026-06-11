/**
 * @file rsbs_version.h
 * @brief RedShipBlueShip release identity (issue #319)
 *
 * This is RSBS's own version, independent of the upstream-derived
 * project(Ship VERSION 9.1.1) in the root CMakeLists.txt. That version is
 * baked into OoT_gBuildVersion* / MM_gBuildVersion* and validates .o2r
 * archives and save files against the build (OTRGlobals.cpp, BenPort.cpp,
 * SaveManager.cpp), so changing it would invalidate every existing archive.
 * RSBS identity therefore lives here instead.
 *
 * Release tags must equal RSBS_VERSION_STRING — the v* tag is what triggers
 * the release pipeline in .github/workflows/generate-builds.yml.
 *
 * Codename scheme per docs/VERSIONING.md: film character for the major
 * release, NATO phonetic word indexed by the patch number.
 */

#ifndef RSBS_VERSION_H
#define RSBS_VERSION_H

#define RSBS_VERSION_MAJOR 0
#define RSBS_VERSION_MINOR 1
#define RSBS_VERSION_PATCH 0
#define RSBS_VERSION_STAGE "prealpha"
#define RSBS_VERSION_CODENAME "Morpheus Alfa"

#define RSBS_STRINGIFY_(x) #x
#define RSBS_STRINGIFY(x) RSBS_STRINGIFY_(x)

/* "v0.1.0-prealpha" — must equal the git release tag. */
#define RSBS_VERSION_STRING                                                                       \
    "v" RSBS_STRINGIFY(RSBS_VERSION_MAJOR) "." RSBS_STRINGIFY(RSBS_VERSION_MINOR) "." RSBS_STRINGIFY( \
        RSBS_VERSION_PATCH) "-" RSBS_VERSION_STAGE

#define RSBS_APP_NAME "RedShipBlueShip"

/* Context display name: window title, crash-dialog branding, log name. */
#define RSBS_WINDOW_TITLE RSBS_APP_NAME " " RSBS_VERSION_STRING " (" RSBS_VERSION_CODENAME ")"

#endif // RSBS_VERSION_H
