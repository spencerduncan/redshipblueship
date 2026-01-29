#include "ultra64.h"

void* MM_osViGetCurrentFramebuffer(void) {
    register u32 prevInt = MM___osDisableInt();
    void* curBuf = __osViCurr->buffer;

    MM___osRestoreInt(prevInt);

    return curBuf;
}
