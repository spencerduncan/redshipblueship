/**
 * Windows import shims for SharedGraphics API
 *
 * libultraship declares Combo_GetSharedGraphics and Combo_SetSharedGraphics
 * with __declspec(dllimport), which creates references to __imp_* symbols.
 * We provide those import pointers here, pointing to our implementations.
 */

#ifdef _WIN32

#include "combo/SharedGraphics.h"

// Import thunks - these are what __declspec(dllimport) references look for
// The naming convention is __imp_<function_name>
extern "C" {
    // Pointers that the dllimport references will resolve to
    decltype(&Combo_GetSharedGraphics) __imp_Combo_GetSharedGraphics = &Combo_GetSharedGraphics;
    decltype(&Combo_SetSharedGraphics) __imp_Combo_SetSharedGraphics = &Combo_SetSharedGraphics;
}

#endif // _WIN32
