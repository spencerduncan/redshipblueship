#include "ultra64.h"
#include "alignment.h"
#include "macros.h"

u32 __osSiAccessQueueEnabled = 0;

OSMesg siAccessBuf[1] ALIGNED(8);
OSMesgQueue __osSiAccessQueue ALIGNED(8);

void __osSiCreateAccessQueue() {
    __osSiAccessQueueEnabled = 1;
    MM_osCreateMesgQueue(&__osSiAccessQueue, siAccessBuf, ARRAY_COUNT(siAccessBuf));
    MM_osSendMesg(&__osSiAccessQueue, NULL, OS_MESG_NOBLOCK);
}

void __osSiGetAccess(void) {
    OSMesg dummyMesg;

    if (!__osSiAccessQueueEnabled) {
        __osSiCreateAccessQueue();
    }
    MM_osRecvMesg(&__osSiAccessQueue, &dummyMesg, OS_MESG_BLOCK);
}

void __osSiRelAccess(void) {
    MM_osSendMesg(&__osSiAccessQueue, NULL, OS_MESG_NOBLOCK);
}
