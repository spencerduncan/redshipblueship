#include "global.h"

OSPiHandle* sISVHandle; // official name : is_Handle

#define gISVDbgPrnAdrs ((ISVDbg*)0xB3FF0000)
#define ASCII_TO_U32(a, b, c, d) ((u32)((a << 24) | (b << 16) | (c << 8) | (d << 0)))

void isPrintfInit(void) {
    sISVHandle = OoT_osCartRomInit();
    OoT_osEPiWriteIo(sISVHandle, (u32)&gISVDbgPrnAdrs->put, 0);
    OoT_osEPiWriteIo(sISVHandle, (u32)&gISVDbgPrnAdrs->get, 0);
    OoT_osEPiWriteIo(sISVHandle, (u32)&gISVDbgPrnAdrs->magic, ASCII_TO_U32('I', 'S', '6', '4'));
}

void OoT_osSyncPrintfUnused(const char* fmt, ...) {
}

/*void osSyncPrintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    _Printf(is_proutSyncPrintf, NULL, fmt, args);

    va_end(args);
}*/

// assumption
void OoT_rmonPrintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    _Printf(is_proutSyncPrintf, NULL, fmt, args);

    va_end(args);
}

void* is_proutSyncPrintf(void* arg, const char* str, u32 count) {
    u32 data;
    s32 pos;
    s32 start;
    s32 end;

    // OTRLogString(str);

    OoT_osEPiReadIo(sISVHandle, (u32)&gISVDbgPrnAdrs->magic, &data);
    if (data != ASCII_TO_U32('I', 'S', '6', '4')) {
        return 1;
    }
    OoT_osEPiReadIo(sISVHandle, (u32)&gISVDbgPrnAdrs->get, &data);
    pos = data;
    OoT_osEPiReadIo(sISVHandle, (u32)&gISVDbgPrnAdrs->put, &data);
    start = data;
    end = start + count;
    if (end >= 0xFFE0) {
        end -= 0xFFE0;
        if (pos < end || start < pos) {
            return 1;
        }
    } else {
        if (start < pos && pos < end) {
            return 1;
        }
    }
    while (count) {
        u32 addr = (u32)&gISVDbgPrnAdrs->data + (start & 0xFFFFFFC);
        s32 shift = ((3 - (start & 3)) * 8);

        if (*str) {
            OoT_osEPiReadIo(sISVHandle, addr, &data);
            OoT_osEPiWriteIo(sISVHandle, addr, (*str << shift) | (data & ~(0xFF << shift)));

            start++;
            if (start >= 0xFFE0) {
                start -= 0xFFE0;
            }
        }
        count--;
        str++;
    }
    OoT_osEPiWriteIo(sISVHandle, (u32)&gISVDbgPrnAdrs->put, start);

    return 1;
}

void func_80002384(const char* exp, const char* file, u32 line) {
    osSyncPrintf("File:%s Line:%d  %s \n", file, line, exp);
    while (true) {
        ;
    }
}
