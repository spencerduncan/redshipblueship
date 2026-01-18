#include "global.h"

#ifndef __GNUC__
#define __builtin_sqrtf OoT_sqrtf
#endif

f32 OoT_sqrtf(f32 f) {
    return __builtin_sqrtf(f);
}
