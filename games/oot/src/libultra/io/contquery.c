#include "global.h"

/**
 * OoT_osContStartQuery:
 * Starts to read the values for SI device status and type which are connected to the controller port and joyport
 * connector.
 */
s32 OoT_osContStartQuery(OSMesgQueue* mq) {
    s32 ret = 0;

    __osSiGetAccess();
    if (__osContLastPoll != CONT_CMD_REQUEST_STATUS) {
        __osPackRequestData(CONT_CMD_REQUEST_STATUS);
        ret = __osSiRawStartDma(OS_WRITE, &__osPifInternalBuff);
        OoT_osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    }
    ret = __osSiRawStartDma(OS_READ, &__osPifInternalBuff);
    __osContLastPoll = CONT_CMD_REQUEST_STATUS;
    __osSiRelAccess();
    return ret;
}

/**
 * OoT_osContGetQuery:
 * Returns the values from OoT_osContStartQuery to status. Both functions must be paired for use.
 */
void OoT_osContGetQuery(OSContStatus* data) {
    u8 pattern;
    __osContGetInitData(&pattern, data);
}
