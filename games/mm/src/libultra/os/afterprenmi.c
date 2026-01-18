#include "ultra64.h"

s32 MM_osAfterPreNMI(void) {
    return __osSpSetPc(0);
}
