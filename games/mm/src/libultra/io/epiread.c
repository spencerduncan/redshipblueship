#include "ultra64.h"

s32 MM_osEPiReadIo(OSPiHandle* handle, uintptr_t devAddr, u32* data) {
    register s32 ret;

    __osPiGetAccess();
    ret = __osEPiRawReadIo(handle, devAddr, data);
    __osPiRelAccess();

    return ret;
}
