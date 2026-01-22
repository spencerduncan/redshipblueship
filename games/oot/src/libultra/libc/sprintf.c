//#include "global.h"
//
// void* OoT_proutSprintf(void* dst, const char* fmt, u32 size) {
//    return (void*)((u32)memcpy(dst, fmt, size) + size);
//}
//
// s32 OoT_vsprintf(char* dst, const char* fmt, va_list args) {
//    s32 ret = _Printf(OoT_proutSprintf, dst, fmt, args);
//    if (ret > -1) {
//        dst[ret] = 0;
//    }
//    return ret;
//}
//
// s32 sprintf(char* dst, const char* fmt, ...) {
//    s32 ret;
//    va_list args;
//    va_start(args, fmt);
//
//    ret = _Printf(OoT_proutSprintf, dst, fmt, args);
//    if (ret > -1) {
//        dst[ret] = 0;
//    }
//
//    va_end(args);
//
//    return ret;
//}
