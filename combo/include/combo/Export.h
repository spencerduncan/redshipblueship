#pragma once

/**
 * DLL Export/Import macros for Windows
 *
 * On Windows:
 * - When building combo.dll (COMBO_BUILDING_DLL), functions are exported
 * - When building game DLLs that use combo, functions are imported
 *
 * On Unix:
 * - When building the exe (COMBO_BUILDING_EXE), functions get default visibility
 * - The exe uses -rdynamic to export symbols for loaded shared libraries
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
