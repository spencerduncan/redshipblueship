#include "ultra64.h"

void MM_osViSetMode(OSViMode* modep) {
    register u32 saveMask = MM___osDisableInt();

    __osViNext->modep = modep;
    __osViNext->state = VI_STATE_MODE_UPDATED;
    __osViNext->features = __osViNext->modep->comRegs.ctrl;

    MM___osRestoreInt(saveMask);
}
