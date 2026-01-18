#include "ultra64.h"

void MM_osDpSetStatus(u32 data) {
    IO_WRITE(DPC_STATUS_REG, data);
}
