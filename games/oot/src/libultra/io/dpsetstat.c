#include "global.h"

void OoT_osDpSetStatus(u32 status) {
    DPC_STATUS_REG = status;
}
