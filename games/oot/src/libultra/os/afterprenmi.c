#include <libultraship/libultra.h>
#include "global.h"

s32 OoT_osAfterPreNMI(void) {
    return __osSpSetPc(0);
}
