#include "ultra64.h"

void MM_osViSwapBuffer(void* frameBufPtr) {

    u32 saveMask = MM___osDisableInt();

    __osViNext->buffer = frameBufPtr;
    __osViNext->state |= VI_STATE_BUFFER_UPDATED;

    MM___osRestoreInt(saveMask);
}
