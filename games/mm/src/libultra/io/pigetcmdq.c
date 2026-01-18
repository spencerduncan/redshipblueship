#include "ultra64.h"

OSMesgQueue* MM_osPiGetCmdQueue(void) {
    if (!__osPiDevMgr.active) {
        return NULL;
    }

    return __osPiDevMgr.cmdQueue;
}
