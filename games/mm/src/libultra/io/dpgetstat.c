#include "ultra64.h"

u32 MM_osDpGetStatus(void) {
    return IO_READ(DPC_STATUS_REG);
}
