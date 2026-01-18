#include "ultra64.h"

OSPri MM_osGetThreadPri(OSThread* t) {
    if (t == NULL) {
        t = __osRunningThread;
    }
    return t->priority;
}
