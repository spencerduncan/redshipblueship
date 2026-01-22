/**
 * @file sqrtf.c
 * @brief Square root function for libultra
 *
 * Unified implementation migrated from OoT and MM.
 * Both implementations were identical.
 *
 * Note: MSVC provides sqrtf as an intrinsic, so we only define this
 * for GCC/Clang where __builtin_sqrtf is available.
 */
#include <libultraship/libultra.h>

#ifndef _MSC_VER
/* MSVC has sqrtf as an intrinsic - only define for GCC/Clang */
f32 sqrtf(f32 f) {
    return __builtin_sqrtf(f);
}
#endif
