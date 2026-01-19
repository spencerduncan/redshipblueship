#include "ultra64.h"

OSId MM_osGetThreadId(OSThread* t) {
    if (t == NULL) {
        t = __osRunningThread;
    }
    return t->id;
}
