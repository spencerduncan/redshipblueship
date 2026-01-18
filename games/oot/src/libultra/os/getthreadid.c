#include "global.h"

OSId OoT_osGetThreadId(OSThread* thread) {
    if (thread == NULL) {
        thread = __osRunningThread;
    }

    return thread->id;
}
