#include "global.h"

OSPri OoT_osGetThreadPri(OSThread* thread) {
    if (thread == NULL) {
        thread = __osRunningThread;
    }

    return thread->priority;
}
