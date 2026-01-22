/**
 * @file seteventmesg.c
 * @brief OS event message setup - unified from OoT/MM decomp
 *
 * Original sources:
 *   - games/oot/src/libultra/os/seteventmesg.c
 *   - games/mm/src/libultra/os/seteventmesg.c
 */

#include "ultra64.h"

__OSEventState __osEventStateTab[OS_NUM_EVENTS];

u32 __osPreNMI = 0;

void osSetEventMesg(OSEvent e, OSMesgQueue* mq, OSMesg msg) {
    register u32 saveMask = __osDisableInt();
    __OSEventState* es = &__osEventStateTab[e];

    es->queue = mq;
    es->msg = msg;

    if (e == OS_EVENT_PRENMI) {
        if (__osShutdown && !__osPreNMI) {
            osSendMesg(mq, msg, OS_MESG_NOBLOCK);
        }
        __osPreNMI = 1;
    }

    __osRestoreInt(saveMask);
}
