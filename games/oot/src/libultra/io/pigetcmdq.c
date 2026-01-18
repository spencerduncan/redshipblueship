#include "global.h"
#include "ultra64/internal.h"

OSMesgQueue* OoT_osPiGetCmdQueue(void) {
    if (!__osPiDevMgr.initialized) {
        return NULL;
    }

    return __osPiDevMgr.cmdQueue;
}
