/**
 * @file getthreadid.c
 * @brief Get thread ID
 *
 * Unified implementation from OoT and MM (functionally identical).
 */

#include "rsbs/libultra_os.h"

OSId osGetThreadId(OSThread* thread) {
    if (thread == NULL) {
        thread = __osRunningThread;
    }
    return thread->id;
}
