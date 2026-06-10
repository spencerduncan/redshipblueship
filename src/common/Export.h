#pragma once

/**
 * DLL Export/Import macros for Windows
 *
 * On Windows:
 * - When building redship_common (COMBO_BUILDING_DLL), functions are exported
 * - Otherwise, functions are imported
 *
 * On Unix:
 * - Functions get default visibility; the exe uses -rdynamic to export
 *   symbols for any loaded shared libraries.
 *
 * Relocated from combo/include/combo/Export.h in Phase 2 T10 (#265) when the
 * legacy combo/ directory was removed. The COMBO_API macro name and the
 * COMBO_BUILDING_DLL define are intentionally preserved (no rename) so this is
 * a pure no-semantic-change move — the only live consumer is
 * src/common/SharedGraphics.h.
 */

#ifdef _WIN32
    #ifdef COMBO_BUILDING_DLL
        #define COMBO_API __declspec(dllexport)
    #else
        #define COMBO_API __declspec(dllimport)
    #endif
#else
    #define COMBO_API
#endif
