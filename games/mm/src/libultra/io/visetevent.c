#include "ultra64.h"

void MM_osViSetEvent(OSMesgQueue* mq, OSMesg m, u32 retraceCount) {
    register u32 saveMask = MM___osDisableInt();

    __osViNext->mq = mq;
    __osViNext->msg = m;
    __osViNext->retraceCount = retraceCount;

    MM___osRestoreInt(saveMask);
}
