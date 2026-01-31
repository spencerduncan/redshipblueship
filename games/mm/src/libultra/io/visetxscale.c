#include "ultra64.h"

void MM_osViSetXScale(f32 value) {
    register u32 nomValue;
    register u32 saveMask = MM___osDisableInt();

    __osViNext->x.factor = value;

    __osViNext->state |= VI_STATE_XSCALE_UPDATED;

    nomValue = __osViNext->modep->comRegs.xScale & VI_SCALE_MASK;
    __osViNext->x.scale = (u32)(__osViNext->x.factor * nomValue) & VI_SCALE_MASK;

    MM___osRestoreInt(saveMask);
}
