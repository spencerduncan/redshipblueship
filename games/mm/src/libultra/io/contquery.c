#include "ultra64.h"
#include "PR/controller.h"

s32 MM_osContStartQuery(OSMesgQueue* mq) {
    s32 ret;

    __osSiGetAccess();

    if (__osContLastPoll != CONT_CMD_REQUEST_STATUS) {
        __osPackRequestData(CONT_CMD_REQUEST_STATUS);
        __osSiRawStartDma(OS_WRITE, &__osContPifRam);
        MM_osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    }

    ret = __osSiRawStartDma(OS_READ, &__osContPifRam);

    __osContLastPoll = CONT_CMD_REQUEST_STATUS;

    __osSiRelAccess();

    return ret;
}

void MM_osContGetQuery(OSContStatus* data) {
    u8 pattern;

    __osContGetInitData(&pattern, data);
}
