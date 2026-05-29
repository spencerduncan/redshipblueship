#pragma once

/**
 * SharedGraphics C API
 *
 * Allows multiple game DLLs to share a single SDL window and GL context.
 * This is used when switching between OoT and MM in the combo launcher.
 *
 * C API (not C++) is required for safe cross-DLL boundary calls on Windows.
 */

#include "combo/Export.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Store SDL window ID and GL context for sharing between games.
 *
 * @param sdlWindowID The SDL window ID (from SDL_GetWindowID). 0 is invalid.
 * @param glContext   The OpenGL context pointer (from SDL_GL_CreateContext).
 */
COMBO_API void Combo_SetSharedGraphics(uint32_t sdlWindowID, void* glContext);

/**
 * Get shared graphics if available.
 *
 * @param sdlWindowID Out pointer for the SDL window ID.
 * @param glContext   Out pointer for the GL context.
 * @return true if valid shared graphics are available, false otherwise.
 */
COMBO_API bool Combo_GetSharedGraphics(uint32_t* sdlWindowID, void** glContext);

/**
 * Check if graphics are currently shared.
 *
 * @return true if shared graphics are set and valid.
 */
COMBO_API bool Combo_HasSharedGraphics(void);

/**
 * Clear shared graphics (call on full shutdown).
 */
COMBO_API void Combo_ClearSharedGraphics(void);

#ifdef __cplusplus
}
#endif