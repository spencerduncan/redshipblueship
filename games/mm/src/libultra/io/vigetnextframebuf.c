#include "ultra64.h"

void* MM_osViGetNextFramebuffer(void) {
    register u32 saveMask = MM___osDisableInt();
    void* buffer;

    buffer = __osViNext->buffer;

    MM___osRestoreInt(saveMask);

    return buffer;
}
