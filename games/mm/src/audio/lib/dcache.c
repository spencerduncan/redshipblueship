#include "global.h"

void MM_Audio_InvalDCache(void* buf, size_t size) {
    OSIntMask prevMask = MM_osSetIntMask(1);

    osInvalDCache(buf, size);
    MM_osSetIntMask(prevMask);
}

void MM_Audio_WritebackDCache(void* buf, size_t size) {
    OSIntMask prevMask = MM_osSetIntMask(1);

    osWritebackDCache(buf, size);
    MM_osSetIntMask(prevMask);
}
