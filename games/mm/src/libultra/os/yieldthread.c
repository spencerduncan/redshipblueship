#include "ultra64.h"

void MM_osYieldThread(void) {
    register u32 saveMask = MM___osDisableInt();

    __osRunningThread->state = OS_STATE_RUNNABLE;
    __osEnqueueAndYield(&__osRunQueue);

    MM___osRestoreInt(saveMask);
}
