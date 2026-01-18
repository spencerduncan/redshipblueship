#include "ultra64.h"
#include "macros.h"

u32 __osPiAccessQueueEnabled = 0;

OSMesg MM_piAccessBuf[1];
OSMesgQueue __osPiAccessQueue;

void __osPiCreateAccessQueue(void) {
    __osPiAccessQueueEnabled = 1;
    MM_osCreateMesgQueue(&__osPiAccessQueue, MM_piAccessBuf, ARRAY_COUNT(MM_piAccessBuf));
    MM_osSendMesg(&__osPiAccessQueue, NULL, OS_MESG_NOBLOCK);
}

void __osPiGetAccess(void) {
    OSMesg dummyMesg;

    if (!__osPiAccessQueueEnabled) {
        __osPiCreateAccessQueue();
    }
    MM_osRecvMesg(&__osPiAccessQueue, &dummyMesg, OS_MESG_BLOCK);
}

void __osPiRelAccess(void) {
    MM_osSendMesg(&__osPiAccessQueue, NULL, OS_MESG_NOBLOCK);
}
