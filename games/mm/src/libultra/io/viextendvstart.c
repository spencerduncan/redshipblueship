#include "ultra64.h"

void MM_osViExtendVStart(u32 value) {
    __additional_scanline = value;
}
