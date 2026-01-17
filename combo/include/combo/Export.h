#pragma once

/**
 * DLL Export/Import macros for Windows
 *
 * When building the redship executable (which contains the combo library),
 * functions are exported. When building game DLLs that call these functions,
 * they're imported.
 */

#ifdef _WIN32
    #ifdef COMBO_BUILDING_EXE
        #define COMBO_API __declspec(dllexport)
    #else
        #define COMBO_API __declspec(dllimport)
    #endif
#else
    #define COMBO_API
#endif
