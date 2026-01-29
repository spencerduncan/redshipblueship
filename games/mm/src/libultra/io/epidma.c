#include "ultra64.h"

s32 MM_osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction) {
    register s32 result;

    if (!__osPiDevMgr.active) {
        return -1;
    }

    mb->piHandle = pihandle;

    if (direction == OS_READ) {
        mb->hdr.type = OS_MESG_TYPE_EDMAREAD;
    } else {
        mb->hdr.type = OS_MESG_TYPE_EDMAWRITE;
    }

    if (mb->hdr.pri == OS_MESG_PRI_HIGH) {
        result = MM_osJamMesg(MM_osPiGetCmdQueue(), mb, OS_MESG_NOBLOCK);
    } else {
        result = MM_osSendMesg(MM_osPiGetCmdQueue(), mb, OS_MESG_NOBLOCK);
    }
    return result;
}
