#include "ultra64.h"

void MM_osSpTaskYield(void) {
    __osSpSetStatus(SP_SET_YIELD);
}
