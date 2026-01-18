#include "global.h"

s32 OoT_osEPiReadIo(OSPiHandle* handle, u32 devAddr, u32* data) {
    register s32 ret;

    __osPiGetAccess();
    ret = __osEPiRawReadIo(handle, devAddr, data);
    __osPiRelAccess();

    return ret;
}
