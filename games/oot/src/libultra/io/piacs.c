#include "global.h"

u32 __osPiAccessQueueEnabled = 0;

OSMesg OoT_piAccessBuf;
OSMesgQueue __osPiAccessQueue;

void __osPiCreateAccessQueue(void) {
    __osPiAccessQueueEnabled = 1;
    OoT_osCreateMesgQueue(&__osPiAccessQueue, &OoT_piAccessBuf, 1);
    OoT_osSendMesg(&__osPiAccessQueue, NULL, OS_MESG_NOBLOCK);
}

void __osPiGetAccess(void) {
    OSMesg mesg;

    if (!__osPiAccessQueueEnabled) {
        __osPiCreateAccessQueue();
    }

    OoT_osRecvMesg(&__osPiAccessQueue, &mesg, OS_MESG_BLOCK);
}

void __osPiRelAccess(void) {
    OoT_osSendMesg(&__osPiAccessQueue, NULL, OS_MESG_NOBLOCK);
}
