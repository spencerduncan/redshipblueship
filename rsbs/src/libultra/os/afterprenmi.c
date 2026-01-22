/**
 * @file afterprenmi.c
 * @brief Unified osAfterPreNMI implementation for OoT and MM
 *
 * Migrated from games/oot/src/libultra/os/afterprenmi.c
 *           and games/mm/src/libultra/os/afterprenmi.c
 */
#include <libultraship/libultra.h>

extern s32 __osSpSetPc(u32 pc);

s32 osAfterPreNMI(void) {
    return __osSpSetPc(0);
}
