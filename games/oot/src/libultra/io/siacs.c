#include "global.h"

OSMesg osSiMesgBuff[SIAccessQueueSize];
OSMesgQueue gOSSiMessageQueue;
u32 gOSSiAccessQueueCreated = 0;

void __osSiCreateAccessQueue(void) {
    gOSSiAccessQueueCreated = 1;
    OoT_osCreateMesgQueue(&gOSSiMessageQueue, &osSiMesgBuff[0], SIAccessQueueSize - 1);
    OoT_osSendMesg(&gOSSiMessageQueue, NULL, OS_MESG_NOBLOCK);
}

void __osSiGetAccess(void) {
    OSMesg mesg;

    if (!gOSSiAccessQueueCreated) {
        __osSiCreateAccessQueue();
    }
    OoT_osRecvMesg(&gOSSiMessageQueue, &mesg, OS_MESG_BLOCK);
}

void __osSiRelAccess(void) {
    OoT_osSendMesg(&gOSSiMessageQueue, NULL, OS_MESG_NOBLOCK);
}
