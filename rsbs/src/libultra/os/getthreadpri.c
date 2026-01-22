/**
 * @file getthreadpri.c
 * @brief Get thread priority
 *
 * Unified implementation from OoT and MM (functionally identical).
 */

#include "rsbs/libultra_os.h"

OSPri osGetThreadPri(OSThread* thread) {
    if (thread == NULL) {
        thread = __osRunningThread;
    }
    return thread->priority;
}
