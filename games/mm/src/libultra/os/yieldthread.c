#include "ultra64.h"

void MM_osYieldThread(void) {
    register u32 saveMask = __osDisableInt();

    __osRunningThread->state = OS_STATE_RUNNABLE;
    __osEnqueueAndYield(&__osRunQueue);

    __osRestoreInt(saveMask);
}
