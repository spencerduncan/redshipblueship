/**
 * @file getactivequeue.c
 * @brief Unified __osGetActiveQueue implementation for OoT and MM
 *
 * Migrated from games/oot/src/libultra/os/getactivequeue.c
 *           and games/mm/src/libultra/os/getactivequeue.c
 */
#include <libultraship/libultra.h>

extern OSThread* __osActiveQueue;

OSThread* __osGetActiveQueue(void) {
    return __osActiveQueue;
}
