#include "ultra64.h"

void* MM_osViGetNextFramebuffer(void) {
    register u32 saveMask = __osDisableInt();
    void* buffer;

    buffer = __osViNext->buffer;

    __osRestoreInt(saveMask);

    return buffer;
}
