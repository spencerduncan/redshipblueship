/**
 * @file mm_creation_new_file_test.cpp
 * @brief The paired creation event authors MM's half the way MM's own
 *        file-select new-file path would (#765).
 *
 * ============================================================================
 * WHAT WAS WRONG
 * ============================================================================
 *
 * MM_Rando_AuthorHalfAtCreation memsets the whole SaveContext, runs
 * MM_Sram_InitNewSave and MM's generation, and arms the result as the MM
 * shadow. MM's own file select (MM_Sram_InitSave, z_sram_NES.c) does the same
 * InitNewSave but then stamps the 'ZELDA3' new-file marker and the checksum,
 * and it runs in a context whose fileNum and flashSaveAvailable the title chain
 * and the naming screen set. The creation event stamped none of them, so the
 * armed half reached every reader with:
 *   - newf all zero: the .redsave panel's "started" flag and the Combo Tracker's
 *     MM presence gate both read "no MM save" (#765, found by #755);
 *   - fileNum 0 and flashSaveAvailable 0: the arrival consumes the WHOLE
 *     SaveContext from the shadow, so these replaced the cross-game session's
 *     0xFF sentinel and its "flash available" for the life of the file. fileNum
 *     0 is a real flash slot, so the moon-crash reset reloaded slot 0 through
 *     the single-exe read stub (which returns nothing) and copied the zeroed
 *     buffer over the live save.
 * And MM's slot-metadata descriptor was never registered in this binary at all
 * (its only registrar lives in the excluded 2s2h/SaveManager/SaveManager.cpp),
 * so every slot's MM half read "not started" whatever its bytes held.
 *
 * ============================================================================
 * TWO ROWS
 * ============================================================================
 *
 *   mm-creation-new-file (redship tier, ROM-free). A SYNTHETIC authoring: the
 *   creation event's own steps minus the generation (memset, MM_Sram_InitNewSave,
 *   the production stamp MM_Creation_StampNewFileFields, a stand-in for
 *   OnFileCreate's rando stamp), armed by the production MM_Rando_ArmCreatedHalf.
 *   Checks the armed shadow's marker, checksum parity, fileNum and flash flag;
 *   the Combo Tracker reads it present by the marker ALONE (a rando-typed shadow
 *   without the marker reads UNAVAILABLE: #755's widening is gone), labelled "As
 *   of file creation" until MM's first load stamps fileCreatedAt; the .redsave
 *   slot panel reports MM started through MM_SlotMeta_Register (and not for an
 *   all-zero MM half); and the moon-crash reset over the consumed half keeps
 *   the save.
 *
 *   mm-creation-new-file-world (rando tier). The PRODUCTION creation event
 *   (OoT_Creation_AuthorRandoFile) over the ComboSingleBag pinned seed
 *   RSBSSINGLEBAG1, then the same checks over the armed shadow it leaves.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "combo_tracker_view.h"
#include "context.h"
#include "game.h"
#include "save.h"

extern "C" {
void MM_Creation_StampNewFileFields(void);
void MM_SlotMeta_Register(void);
int MM_Rando_ArmCreatedHalf(int slot);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);
int OoT_Creation_AuthorRandoFile(int slot);
int Rando_HeadlessSeedTest(const char* seedStr);
void Randomizer_TestClearOoTSave(void);
void Combo_SingleBag_Forget(void);
void Combo_Crossings_Clear(void);
}

namespace {

#define CNF_ASSERT(cond, msg)                                                                     \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            printf("[TEST] FAIL (%s): %s  [%s] (%s:%d)\n", sRow, msg, #cond, __FILE__, __LINE__); \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

const char* sRow = "mm-creation-new-file";
const char* const kSaveDir = "rsbs_test_saves_creation_new_file";
const u8 kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };

// The flash funnel's saveBuf and a copy of the armed half. Static: 128 KiB and
// ~64 KiB do not belong on the test's stack.
u8 sSaveBuf[SAVE_BUFFER_SIZE];
SaveContext sHalf;

void ReadArmedHalf() {
    memcpy(&sHalf, Context_GetMMSaveContext(), sizeof(SaveContext));
}

void PrintHalf(const char* when) {
    const u8* n = reinterpret_cast<const u8*>(sHalf.save.saveInfo.playerData.newf);
    printf("[TEST] %s: %s: newf=%02X %02X %02X %02X %02X %02X saveType=%d fileNum=0x%X flashSaveAvailable=%d "
           "checksum=0x%04X fileCreatedAt=%llu finalSeed=%08X\n",
           sRow, when, n[0], n[1], n[2], n[3], n[4], n[5], (int)sHalf.save.shipSaveInfo.saveType,
           (unsigned)sHalf.fileNum, (int)sHalf.flashSaveAvailable, (unsigned)sHalf.save.saveInfo.checksum,
           (unsigned long long)sHalf.save.shipSaveInfo.fileCreatedAt,
           (unsigned)sHalf.save.shipSaveInfo.rando.finalSeed);
}

/** The MM half in the shadow, overwritten with `bytes` (tracker/meta probes). */
void PutShadow(const std::vector<uint8_t>& bytes) {
    Context_UpdateShadowCopy(GAME_MM, bytes.data(), bytes.size());
}

/**
 * Everything both rows check about the armed half the creation left in the MM
 * shadow. `expectSeed` is the half's MM final seed.
 */
int CheckArmedHalf(uint32_t expectSeed) {
    ReadArmedHalf();
    PrintHalf("the armed MM half");

    // ---- 1. the fields MM's own new-file path stamps ------------------------
    CNF_ASSERT(memcmp(sHalf.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0,
               "the armed half carries MM's 'ZELDA3' new-file marker");
    CNF_ASSERT(sHalf.save.shipSaveInfo.saveType == SAVETYPE_RANDO, "the armed half is a rando save");
    CNF_ASSERT(sHalf.fileNum == 0xFF, "the armed half carries the cross-game 0xFF fileNum sentinel, not slot 0");
    CNF_ASSERT(sHalf.flashSaveAvailable, "the armed half carries the cross-game session's flashSaveAvailable");
    CNF_ASSERT(sHalf.save.shipSaveInfo.fileCreatedAt == 0, "MM has not loaded the armed half: no creation stamp yet");

    // ---- 2. the Combo Tracker: present by the marker alone -------------------
    MM_TrackerAdapter_Register();
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    CNF_ASSERT(desc != nullptr, "MM's tracker descriptor registered");
    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    printf("[TEST] %s: tracker MM summary: freshness=%d hasWorld=%d seed=%08X shuffled=%d label=\"%s\"\n", sRow,
           (int)summary.freshness, (int)summary.hasWorld, (unsigned)summary.seed, summary.shuffled,
           Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE));
    CNF_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE, "the tracker reads the armed MM half as present");
    CNF_ASSERT(summary.hasWorld && summary.seed == expectSeed, "the tracker reads the armed half's rando world");
    CNF_ASSERT(
        strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE), "As of file creation") == 0,
        "a half MM has never loaded is labelled as of file creation");

    std::vector<uint8_t> armed((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(armed.data(), Context_GetMMSaveContext(), armed.size());
    {
        // #755's widening is gone: a rando save type without the marker is not
        // an MM save (the shape every creation left before #765).
        std::vector<uint8_t> unmarked = armed;
        memset(unmarked.data() + desc->newfOffset, 0, desc->newfLen);
        PutShadow(unmarked);
        Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
        CNF_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE,
                   "presence is the marker's alone: no save-type widening");
        // MM's first load stamps fileCreatedAt (SavingEnhancements' OnSaveLoad);
        // from then on the note names the last switch or save.
        std::vector<uint8_t> loaded = armed;
        const uint64_t createdAt = 1760000000ull;
        memcpy(loaded.data() + offsetof(SaveContext, save.shipSaveInfo.fileCreatedAt), &createdAt, sizeof(createdAt));
        PutShadow(loaded);
        CNF_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE),
                          "As of the last game switch or save") == 0,
                   "a half MM has loaded is labelled as of the last switch or save");
        PutShadow(armed);
    }

    // ---- 3. the .redsave slot panel: MM started -----------------------------
    MM_SlotMeta_Register();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir);
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0); // an erase is what unlatches the slot for this session's write
    CNF_ASSERT(mgr.Save(0), "the slot commits");
    rsbs::SlotMeta meta = mgr.ReadMeta(0);
    printf("[TEST] %s: slot meta: valid=%d ootStarted=%d mmStarted=%d mmName=\"%s\"\n", sRow, (int)meta.valid,
           (int)meta.ootStarted, (int)meta.mmStarted, meta.mmName);
    CNF_ASSERT(meta.valid && meta.mmStarted, "the slot panel reads the paired MM half as started");
    CNF_ASSERT(meta.mmName[0] == '\0', "MM's descriptor pulls no name (MM's charset; the half has none of its own)");
    {
        // Control: an MM half that does not exist reads not started.
        std::vector<uint8_t> zero((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        PutShadow(zero);
        mgr.DeleteSave(0);
        CNF_ASSERT(mgr.Save(0), "the control slot commits");
        meta = mgr.ReadMeta(0);
        CNF_ASSERT(meta.valid && !meta.mmStarted, "an all-zero MM half reads not started");
        PutShadow(armed);
    }
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    return 0;
}

/**
 * The consequence the fileNum stamp exists for: the arrival consumes the armed
 * half into the live save, and the moon-crash reset must not reload a flash slot
 * the session does not have (Sram_FileNumHasFlashSlot) and copy the read stub's
 * empty buffer over it.
 */
int CheckMoonCrashKeepsConsumedHalf() {
    CNF_ASSERT(Combo_ConsumeFrozenState("mm", &gSaveContext, sizeof(gSaveContext)) == 1,
               "the armed half is consumed into the live save, as the arrival does");
    const s32 day = 2;
    const s16 rupees = 0x77;
    gSaveContext.save.day = day;
    gSaveContext.save.saveInfo.playerData.rupees = rupees;
    gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;

    SramContext sramCtx;
    memset(&sramCtx, 0, sizeof(sramCtx));
    memset(sSaveBuf, 0, sizeof(sSaveBuf));
    sramCtx.saveBuf = sSaveBuf;
    Sram_ResetSaveFromMoonCrash(&sramCtx);

    printf("[TEST] %s: after the moon-crash reset: fileNum=0x%X day=%d rupees=%d bottle1=0x%02X newf[0]=%02X\n", sRow,
           (unsigned)gSaveContext.fileNum, (int)gSaveContext.save.day,
           (int)gSaveContext.save.saveInfo.playerData.rupees,
           (unsigned)gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1],
           (unsigned)gSaveContext.save.saveInfo.playerData.newf[0]);
    CNF_ASSERT(gSaveContext.save.day == day && gSaveContext.save.saveInfo.playerData.rupees == rupees,
               "the moon-crash reset keeps the consumed half's save (no reload from a flash slot it does not have)");
    CNF_ASSERT(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] == ITEM_BOTTLE,
               "the moon-crash reset keeps the consumed half's inventory");
    CNF_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0,
               "the moon-crash reset keeps the marker");
    return 0;
}

} // namespace

extern "C" int MM_CreationNewFile_RunSynthetic(void) {
    sRow = "mm-creation-new-file";
    printf("[TEST] %s: a synthetic creation-authored MM half carries what MM's own new-file path stamps (#765)\n",
           sRow);

    const GameId prevGame = Context_GetCurrentGame();
    Context_InitFrozenStates();
    ComboContext_Init();
    Context_ClearAllFrozenStates();

    // The creation event's own authoring, minus the generation it runs between
    // these lines (MM_Rando_AuthorHalfAtCreation).
    const uint32_t kSeed = 0x5EED0765u;
    memset(&gSaveContext, 0, sizeof(SaveContext));
    MM_Sram_InitNewSave();
    // MM_Sram_InitSave's own steps over the SAME bytes (InitNewSave draws its
    // random save fields -- lottery, bomber and spider-house codes -- from the
    // RNG, so a second InitNewSave is a different Save): the 'ZELDA3' marker,
    // then the checksum over Save. The typed name and button 0's cutsceneIndex
    // are the two InitSave steps the creation event does not take (see
    // MM_Creation_StampNewFileFields's header), so they are left out here too.
    static Save sMMPath;
    memcpy(&sMMPath, &gSaveContext.save, sizeof(Save));
    memcpy(sMMPath.saveInfo.playerData.newf, kNewf, sizeof(kNewf));
    sMMPath.saveInfo.checksum = Sram_CalcChecksum(&sMMPath, sizeof(Save));

    MM_Creation_StampNewFileFields();
    printf("[TEST] %s: checksum stamped=0x%04X MM's path=0x%04X\n", sRow, (unsigned)gSaveContext.save.saveInfo.checksum,
           (unsigned)sMMPath.saveInfo.checksum);
    CNF_ASSERT(gSaveContext.save.saveInfo.checksum == sMMPath.saveInfo.checksum,
               "the checksum is the one MM_Sram_InitSave computes");
    CNF_ASSERT(memcmp(&gSaveContext.save, &sMMPath, sizeof(Save)) == 0,
               "the stamped Save is byte-identical to MM_Sram_InitSave's (name and button-0 cutscene aside)");
    // Stand-in for OnFileCreate's rando block (the generation's first stamp).
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = kSeed;

    CNF_ASSERT(MM_Rando_ArmCreatedHalf(0) == 0, "the authored half arms");
    CNF_ASSERT(Context_HasFrozenState(GAME_MM) != 0, "the MM shadow is armed");
    Context_SetCurrentGame(GAME_OOT);

    if (CheckArmedHalf(kSeed) != 0) {
        return 1;
    }
    if (CheckMoonCrashKeepsConsumedHalf() != 0) {
        return 1;
    }

    Context_ClearAllFrozenStates();
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

extern "C" int MM_CreationNewFile_RunWorld(void) {
    sRow = "mm-creation-new-file-world";
    printf("[TEST] %s: the production paired creation over RSBSSINGLEBAG1 arms an MM half MM's own new-file path "
           "would recognize (#765)\n",
           sRow);

    CNF_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAG1") == 0, "the pinned seed generates");
    Randomizer_TestClearOoTSave();
    Context_ClearFrozenState(GAME_MM);
    CNF_ASSERT(OoT_Creation_AuthorRandoFile(0) == 1, "the production creation event succeeds");
    CNF_ASSERT(Context_HasFrozenState(GAME_MM) != 0, "MM's world is in the armed shadow; MM never booted");
    Context_SetCurrentGame(GAME_OOT);

    ReadArmedHalf();
    const uint32_t seed = sHalf.save.shipSaveInfo.rando.finalSeed;
    CNF_ASSERT(seed != 0, "the armed half carries a generated world");
    if (CheckArmedHalf(seed) != 0) {
        return 1;
    }
    if (CheckMoonCrashKeepsConsumedHalf() != 0) {
        return 1;
    }

    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}
