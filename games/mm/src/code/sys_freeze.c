#include "global.h"

NORETURN void Sys_Freeze(void) {
    for (;;) {
        MM_Sleep_Msec(1000);
    }
}
