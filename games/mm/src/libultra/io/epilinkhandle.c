#include "ultra64.h"

s32 osEPiLinkHandle(OSPiHandle* handle) {
    u32 saveMask = MM___osDisableInt();

    handle->next = __osPiTable;
    __osPiTable = handle;

    MM___osRestoreInt(saveMask);

    return 0;
}
