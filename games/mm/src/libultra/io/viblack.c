#include "ultra64.h"

void MM_osViBlack(u8 active) {
    register u32 saveMask = MM___osDisableInt();

    if (active) {
        __osViNext->state |= VI_STATE_BLACK;
    } else {
        __osViNext->state &= ~VI_STATE_BLACK;
    }

    MM___osRestoreInt(saveMask);
}
