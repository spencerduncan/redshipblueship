/**
 * @file mm_flash_filenum_test.cpp
 * ROM-free, display-free lock for the MM flash page-table out-of-bounds read
 * driven by gSaveContext.fileNum == 0xFF. CTest label "redship", row
 * MMFlashFileNumOob in CMake/SingleExecutable.cmake, dispatch "mm-flash-filenum-oob"
 * in src/common/test_runner.cpp.
 *
 * What was broken: a cross-game MM session runs with fileNum == 0xFF (the
 * "no real file slot" sentinel the title/debug/mapselect boot leaves in
 * gSaveContext, and which no cross-game arrival replaces). MM's flash paths
 * index the fixed-size gFlashSave*Pages / gFlashOwlSave*Pages tables by
 * `fileNum * FLASH_SAVE_MAIN_MULTIPLIER`, so 0xFF subscripts them at 510 --
 * hundreds of entries past the end. On the moon-crash reset path
 * (Sram_ResetSaveFromMoonCrash, plus DeleteOwlSave -> func_80147314 fired from
 * the BeforeMoonCrashSaveReset hook) that wild index becomes a flash page
 * number/count and the reset then copies the resulting garbage over the live
 * save -- the operator's "spawn as Fierce Deity in the opening cutscene with
 * every inventory slot reading as the Ocarina of Time" corruption.
 *
 * The invariant this locks: for every reachable fileNum -- including the 0xFF
 * sentinel -- no flash page-table index is computed out of bounds, and the
 * reported reset paths leave the live save intact when there is no real slot.
 *
 * Three checks, none needing a display or ROM:
 *   1. Bounds invariant. Sram_FileNumHasFlashSlot accepts exactly the fileNums
 *      whose every index form lands inside the real tables (length FLASH_SAVE_MAX
 *      for the main tables, FILE_NUM_MAX*2 for the owl tables), and rejects
 *      0xFF. This pins the guard's accept-set to the actual table extents.
 *   2. Owl-delete write site. func_80147314(&sram, 0xFF) -- the second OOB site
 *      on the moon-crash path -- must bail before touching gFlashOwlSave*Pages
 *      and before mutating the save. A pre-stamped newf marker must survive.
 *   3. Moon-crash reset (the reported path). Sram_ResetSaveFromMoonCrash under
 *      the 0xFF sentinel must not reload from flash and must not overwrite the
 *      live save with the zeroed buffer. A pre-stamped player name and day must
 *      survive.
 *
 * Removing the fix (reverting the func_80147314 / Sram_ResetSaveFromMoonCrash
 * guards) flips checks 2 and 3 to failure without any crash: the OOB reads land
 * in adjacent .data, the funnel rejects the garbage pages as unavailable, and
 * the save is corrupted exactly as observed.
 */

#include "global.h"

#include <cstdio>
#include <cstring>

namespace {

#define FLASH_ASSERT(cond, msg)                                           \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// Backing store for the flash funnel's saveBuf. Static so the 128 KiB does not
// land on the test's stack.
u8 sSaveBuf[SAVE_BUFFER_SIZE];

} // namespace

extern "C" int MM_FlashFileNumOob_RunHeadless(void) {
    printf("[TEST] mm-flash-filenum-oob: flash page-table indices stay in bounds for fileNum 0xFF\n");

    // ------------------------------------------------------------------
    // Check 1: the guard's accept-set matches the real table extents.
    // The main page tables have FLASH_SAVE_MAX entries; the owl page tables
    // have FILE_NUM_MAX * 2. Sram_FileNumHasFlashSlot must accept a fileNum iff
    // every index the flash paths derive from it lands inside those tables.
    // ------------------------------------------------------------------
    for (s32 fileNum = 0; fileNum <= 0xFF; fileNum++) {
        s32 accepted = Sram_FileNumHasFlashSlot(fileNum);

        // Largest subscripts any flash path forms from fileNum:
        //   main new-cycle + backup:   fileNum * 2 (+1)
        //   main owl (MM_Sram_OpenSave): (fileNum + FILE_NUM_OWL_SAVE_OFFSET) * 2 (+1)
        //   owl tables (func_80147314):  fileNum * 2 (+1)
        s32 mainNewCycle = fileNum * FLASH_SAVE_MAIN_MULTIPLIER + FLASH_SAVE_BACKUP_OFFSET;
        s32 mainOwl = (fileNum + FILE_NUM_OWL_SAVE_OFFSET) * FLASH_SAVE_MAIN_MULTIPLIER + FLASH_SAVE_BACKUP_OFFSET;
        s32 owlTable = fileNum * FLASH_SAVE_MAIN_MULTIPLIER + FLASH_SAVE_BACKUP_OFFSET;

        s32 inBounds = (fileNum >= 0) && (mainNewCycle < FLASH_SAVE_MAX) && (mainOwl < FLASH_SAVE_MAX) &&
                       (owlTable < FILE_NUM_MAX * FLASH_SAVE_MAIN_MULTIPLIER);

        FLASH_ASSERT(accepted == inBounds,
                     "Sram_FileNumHasFlashSlot disagrees with the real flash-table bounds for some fileNum");
    }

    // The sentinel itself, and the legitimate slots, called out explicitly.
    FLASH_ASSERT(!Sram_FileNumHasFlashSlot(0xFF), "0xFF sentinel must be rejected as a flash slot");
    FLASH_ASSERT(Sram_FileNumHasFlashSlot(0) && Sram_FileNumHasFlashSlot(1) && Sram_FileNumHasFlashSlot(2),
                 "real file slots 0..2 must be accepted");
    FLASH_ASSERT(!Sram_FileNumHasFlashSlot(-1) && !Sram_FileNumHasFlashSlot(FILE_NUM_MAX),
                 "out-of-range fileNums must be rejected");

    // ------------------------------------------------------------------
    // Check 2: func_80147314 (owl-save delete) honors the sentinel.
    // This is the write site reached via DeleteOwlSave() from the
    // BeforeMoonCrashSaveReset hook, and via MM_Sram_OpenSave's save-continue
    // path -- both with gSaveContext.fileNum possibly 0xFF.
    // ------------------------------------------------------------------
    {
        SramContext sramCtx;
        memset(&sramCtx, 0, sizeof(sramCtx));
        memset(sSaveBuf, 0, sizeof(sSaveBuf));
        sramCtx.saveBuf = sSaveBuf;

        memset(&gSaveContext, 0, sizeof(gSaveContext));
        gSaveContext.fileNum = 0xFF;

        // A marker that is NOT the 'ZELDA3' valid-file sentinel func_80147314
        // writes on its way out; if the function runs it will overwrite this.
        const char kNewfMarker[6] = { 'R', 'S', 'B', 'S', '!', '?' };
        memcpy(gSaveContext.save.saveInfo.playerData.newf, kNewfMarker, sizeof(kNewfMarker));

        func_80147314(&sramCtx, gSaveContext.fileNum);

        FLASH_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewfMarker, sizeof(kNewfMarker)) == 0,
                     "func_80147314 ran for the 0xFF sentinel -- OOB owl-save page index + save mutation");
    }

    // ------------------------------------------------------------------
    // Check 3: Sram_ResetSaveFromMoonCrash (the reported crash path) preserves
    // the live save under the 0xFF sentinel instead of reloading garbage from
    // an out-of-bounds flash index and copying it over the save.
    // ------------------------------------------------------------------
    {
        SramContext sramCtx;
        memset(&sramCtx, 0, sizeof(sramCtx));
        memset(sSaveBuf, 0, sizeof(sSaveBuf));
        sramCtx.saveBuf = sSaveBuf;

        memset(&gSaveContext, 0, sizeof(gSaveContext));
        gSaveContext.fileNum = 0xFF;

        const char kPlayerName[8] = { 'C', 'R', 'O', 'S', 'S', 'M', 'M', '\0' };
        memcpy(gSaveContext.save.saveInfo.playerData.playerName, kPlayerName, sizeof(kPlayerName));
        gSaveContext.save.day = 0x1234;
        // A valid-file newf, so the CHECK_NEWF backup-reload branch is on the
        // table too; with the fix the whole reload block is skipped.
        const char kValidNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
        memcpy(gSaveContext.save.saveInfo.playerData.newf, kValidNewf, sizeof(kValidNewf));

        Sram_ResetSaveFromMoonCrash(&sramCtx);

        FLASH_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.playerName, kPlayerName, sizeof(kPlayerName)) == 0,
                     "moon-crash reset clobbered the live player name under the 0xFF sentinel");
        FLASH_ASSERT(gSaveContext.save.day == 0x1234,
                     "moon-crash reset wiped save.day under the 0xFF sentinel (zeroed buffer copied over the save)");
    }

    // Leave gSaveContext clean for whatever runs next.
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    printf("[TEST] mm-flash-filenum-oob: PASS\n");
    return 0;
}
