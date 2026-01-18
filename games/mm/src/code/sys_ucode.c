/*
 * File: sys_ucode.c
 * Description: Functions for obtaining locations and sizes of microcode
 */
#include "global.h"
#include <libultraship/bridge/gfxbridge.h>

UcodeHandlers initialgspUcodeText = ucode_f3dex2;
// u64* initialgspUcodeData = gspF3DZEX2_NoN_PosLight_fifoDataStart;

u64* MM_SysUcode_GetUCodeBoot(void) {
    return rspbootTextStart;
}

size_t MM_SysUcode_GetUCodeBootSize(void) {
    return (uintptr_t)rspbootTextEnd - (uintptr_t)rspbootTextStart;
}

UcodeHandlers MM_SysUcode_GetUCode(void) {
    return initialgspUcodeText;
}

// u64* SysUcode_GetUCodeData(void) {
//     return initialgspUcodeData;
// }
