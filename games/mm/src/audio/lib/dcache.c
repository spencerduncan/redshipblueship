#include "global.h"

void MM_Audio_InvalDCache(void* buf, size_t size) {
    OSIntMask prevMask = osSetIntMask(1);

    osInvalDCache(buf, size);
    osSetIntMask(prevMask);
}

void MM_Audio_WritebackDCache(void* buf, size_t size) {
    OSIntMask prevMask = osSetIntMask(1);

    osWritebackDCache(buf, size);
    osSetIntMask(prevMask);
}
