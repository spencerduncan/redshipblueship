#include "global.h"

void OoT_osSpTaskYield(void) {
    __osSpSetStatus(SP_STATUS_SIG3);
}
