#include "global.h"

void* OoT_osViGetNextFramebuffer(void) {
    u32 prevInt = __osDisableInt();
    void* buff = __osViNext->buffer;

    __osRestoreInt(prevInt);
    return buff;
}
