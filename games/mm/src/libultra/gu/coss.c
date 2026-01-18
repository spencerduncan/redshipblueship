#include "ultra64.h"

/**
 * Compute the sine of a hex angle and return a short, using the formula cos(x) = sin(x+pi).
 */
s16 MM_coss(u16 x) {
    return MM_sins(x + 0x4000);
}
