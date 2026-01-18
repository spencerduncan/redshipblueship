#include "global.h"

s32 OoT_osEPiWriteIo(OSPiHandle* handle, u32 devAddr, u32 data) {
    register s32 ret;

    __osPiGetAccess();
    ret = __osEPiRawWriteIo(handle, devAddr, data);
    __osPiRelAccess();

    return ret;
}
