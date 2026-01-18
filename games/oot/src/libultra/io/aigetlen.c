#include "global.h"

u32 OoT_osAiGetLength(void) {
    return HW_REG(AI_LEN_REG, u32);
}
