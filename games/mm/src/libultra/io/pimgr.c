#include "ultra64.h"
#include "alignment.h"
#include "stack.h"
#include "macros.h"

OSPiHandle __Dom1SpeedParam ALIGNED(8);
OSPiHandle __Dom2SpeedParam ALIGNED(8);
OSThread sPiMgrThread;
STACK(sPiMgrStack, 0x1000);
OSMesgQueue piEventQueue ALIGNED(8);
OSMesg MM_piEventBuf[1];

OSDevMgr __osPiDevMgr = { 0 };
OSPiHandle* __osPiTable = NULL;
OSPiHandle* __osCurrentHandle[2] ALIGNED(8) = { &__Dom1SpeedParam, &__Dom2SpeedParam };

void MM_osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, s32 cmdMsgCnt) {
    u32 savedMask;
    OSPri oldPri;
    OSPri myPri;

    if (!__osPiDevMgr.active) {
        MM_osCreateMesgQueue(cmdQ, cmdBuf, cmdMsgCnt);
        MM_osCreateMesgQueue(&piEventQueue, MM_piEventBuf, ARRAY_COUNT(MM_piEventBuf));
        if (!__osPiAccessQueueEnabled) {
            __osPiCreateAccessQueue();
        }
        MM_osSetEventMesg(OS_EVENT_PI, &piEventQueue, (OSMesg)0x22222222);
        oldPri = -1;
        myPri = MM_osGetThreadPri(NULL);
        if (myPri < pri) {
            oldPri = myPri;
            MM_osSetThreadPri(NULL, pri);
        }
        savedMask = __osDisableInt();
        __osPiDevMgr.active = 1;
        __osPiDevMgr.thread = &sPiMgrThread;
        __osPiDevMgr.cmdQueue = cmdQ;
        __osPiDevMgr.evtQueue = &piEventQueue;
        __osPiDevMgr.acsQueue = &__osPiAccessQueue;
        __osPiDevMgr.piDmaCallback = __osPiRawStartDma;
        __osPiDevMgr.epiDmaCallback = __osEPiRawStartDma;
        osCreateThread(&sPiMgrThread, 0, __osDevMgrMain, &__osPiDevMgr, STACK_TOP(sPiMgrStack), pri);
        MM_osStartThread(&sPiMgrThread);
        __osRestoreInt(savedMask);
        if (oldPri != -1) {
            MM_osSetThreadPri(NULL, oldPri);
        }
    }
}
