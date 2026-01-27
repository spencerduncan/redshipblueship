#include "ultra64.h"

void __osSetGlobalIntMask(OSHWIntr mask) {
    register s32 prevInt = MM___osDisableInt();

    __OSGlobalIntMask |= mask;
    MM___osRestoreInt(prevInt);
}
