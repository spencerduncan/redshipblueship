#include "ultra64.h"

void __osResetGlobalIntMask(OSHWIntr mask) {
    register s32 prevInt = MM___osDisableInt();

    __OSGlobalIntMask &= ~(mask & ~OS_IM_RCP);
    MM___osRestoreInt(prevInt);
}
