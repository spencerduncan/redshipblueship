/**
 * MM flash-storage stubs for the single-executable build (#487).
 *
 * games/mm/2s2h/SaveManager/SaveManager.cpp is filtered out of the single-exe
 * link (games/mm/CMakeLists.txt, "Save Manager"), so MM's two flash-storage
 * entry points -- the funnel every SysFlashrom_* call in
 * games/mm/src/code/sys_flashrom.c goes through -- have no body. They were
 * stubbed in src/common/mm_stubs.c as
 *
 *     int SaveManager_SysFlashrom_ReadData(void* dst, int page, int count)  { return 0; }
 *     int SaveManager_SysFlashrom_WriteData(void* src, int page, int count) { return 0; }
 *
 * against real prototypes of `s32 (void*, u32, u32)` and `void (u8*, u32, u32)`
 * (2s2h/SaveManager/SaveManager.h). Two separate faults in that:
 *
 *  1. THE READ LIED. Returning 0 means "read succeeded" while the caller's
 *     buffer still holds whatever it held before -- in every MM caller, the
 *     zeros from the preceding memset. Callers that check the return therefore
 *     never take their backup-file fallback, and callers that ignore it
 *     memcpy a zeroed buffer straight over gSaveContext. That is the mechanism
 *     behind #487: an owl save in a paired rando world zeroed the live
 *     ShipSaveInfo (saveType + the whole rando block) and MM played vanilla
 *     for the rest of the session. The guard in z_sram_NES.c is what saves the
 *     rando half; this return value is what stops the lie at its source, so
 *     every "did the read work?" branch in MM gets the honest answer.
 *     -1 is what the real SaveManager returns for a missing/unparsable file.
 *
 *  2. THE WRITE HAD THE WRONG SIGNATURE -- the mm_stubs.c drift class (#379,
 *     the MotionBlur_Override and OTRConvertHUDXToScreenX faults). It is
 *     benign only because its sole caller (sys_flashrom.c
 *     SysFlashrom_WriteDataAsync) discards the value.
 *
 * Both are fixed here by being HEADER-CHECKED: this TU includes the real
 * SaveManager.h, so the compiler rejects any future drift instead of leaving
 * an ABI mismatch to be discovered at runtime. It lives in the MM target
 * rather than in src/common/mm_stubs.c for two reasons: mm_stubs.c is built
 * into redship_common, which is NOT compiled with RSBS_SINGLE_EXECUTABLE (so
 * the guard below would compile to nothing there and collide with the real
 * SaveManager in a standalone build), and it pulls in no MM headers at all --
 * SaveManager.h's C branch needs MM's u8/u32/s32. Same shape and same reason
 * as mm_framebuffer_effects.c and BenPortHudSingleExe.cpp.
 *
 * NOTE (#487 step 5, deliberately NOT settled here): with the write stub also
 * a no-op, MM's flash storage in single-exe persists nothing at all. Whether
 * `redship` should have a real MM save path is an operator-scoping question,
 * not something this fix decides. Everything here assumes it stays a no-op and
 * makes the no-op honest.
 *
 * Guarded to single-exe: a standalone 2ship build compiles the real
 * SaveManager.cpp, which owns these symbols.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"
#include "2s2h/SaveManager/SaveManager.h"

// Nothing is written, so report failure -- callers must not treat the
// untouched buffer as a loaded save. See the header note above.
s32 SaveManager_SysFlashrom_ReadData(void* addr, u32 pageNum, u32 pageCount) {
    (void)addr;
    (void)pageNum;
    (void)pageCount;
    return -1;
}

void SaveManager_SysFlashrom_WriteData(u8* addr, u32 pageNum, u32 pageCount) {
    (void)addr;
    (void)pageNum;
    (void)pageCount;
}

#endif // RSBS_SINGLE_EXECUTABLE
