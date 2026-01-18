#include "ultra64.h"

#ifndef __GNUC__
#define __builtin_sqrtf MM_sqrtf
#endif

f32 MM_sqrtf(f32 f) {
    return __builtin_sqrtf(f);
}
