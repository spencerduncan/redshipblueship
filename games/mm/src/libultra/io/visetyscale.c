#include "ultra64.h"

void MM_osViSetYScale(f32 value) {
    register u32 saveMask = MM___osDisableInt();

    __osViNext->y.factor = value;

    __osViNext->state |= VI_STATE_YSCALE_UPDATED;

    MM___osRestoreInt(saveMask);
}
