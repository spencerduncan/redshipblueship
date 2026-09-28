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
 *   all-zero MM half); and a moon crash over the consumed half restores the
 *   LAST COMMIT (see "THE MOON CRASH" below).
 *
 *   mm-creation-new-file-world (rando tier). The PRODUCTION creation event
 *   (OoT_Creation_AuthorRandoFile) over the ComboSingleBag pinned seed
 *   RSBSSINGLEBAG1, then the same checks over the armed shadow it leaves.
 *
 * ============================================================================
 * THE MOON CRASH (#785), IN BOTH CREATION ROWS AND ITS OWN ROW
 * ============================================================================
 *
 * Interface_StartMoonCrash sets day 4 / eventDayCount 4 / 06:00 before the
 * crash cutscene, and vanilla's Sram_ResetSaveFromMoonCrash then reloads the
 * file from flash. A paired half runs pinned to fileNum 0xFF, so that reload
 * never ran, nothing replaced it, and #765's lock asserted the half KEPT its
 * pre-crash day: the file stayed on day 4 (no re-trigger, the Final Hours
 * clock, no Dawn of the First Day). The file of a paired half is the .redsave,
 * so the reload is its last whole commit (OoTMM's default "Last Save" moon
 * crash reloads its own save, the foreign save and the shared save together).
 * CheckMoonCrashRestoresLastCommit commits an owl save on day 1 through the
 * production MM funnel, plays on (day 3, more rupees harvested into the pool,
 * a crossing pickup recorded in Tier-1, OoT's half moved), crashes through the
 * REAL Interface_StartMoonCrash and Sram_ResetSaveFromMoonCrash, and asserts the
 * file as last committed: day, time, eventDayCount and inventory of the commit,
 * the half not empty and still this world, Tier-1's post-commit record gone,
 * OoT's half back at the commit, the pool at the committed value, and a later
 * spend harvested as an ordinary delta (the discriminating shared-resource
 * sequence: a watermark left from before the crash would drain the pool).
 *
 *   mm-moon-crash-never-saved (redship tier, ROM-free). A paired half that was
 *   created and never saved: its only commit is the creation's own (OoT's
 *   Sram_InitSave writes the .redsave after the creation event arms the half),
 *   which is vanilla's new-file flash write, so the crash restores the half as
 *   created (day 0, 05:59: Dawn of the First Day). Then a session with nothing
 *   durable at all (no active slot; a latched slot): nothing to reload, the
 *   clock alone restarts at dawn and the half keeps its bytes.
 *
 * ============================================================================
 * THE NAME (#773), IN ALL THREE ROWS
 * ============================================================================
 *
 * MM's own new-file path also copies the name typed on MM's naming screen; the
 * creation event had no name to copy, so every paired half kept MM's default,
 * eight MM spaces, and every MM textbox that names Link printed nothing. The
 * paired world has one name: the creation translates OoT's typed name into MM's
 * charset (OoT_PlayerName_ToMMCharset, OoTMM's copyName mapping) and stamps it
 * where MM_Sram_InitSave stamps the typed name. Both rows above author with a
 * synthetic OoT name (NTSC English "Ab1 z-." plus a voicing mark, which has no
 * MM glyph) and check the half's bytes and the slot panel's decoded name; the
 * synthetic row's byte parity with MM_Sram_InitSave now INCLUDES the name.
 * Both also commit a slot whose OoT half is started with that NTSC name and
 * read it back through the REGISTERED descriptors (OoT_SlotMeta_Register,
 * MM_SlotMeta_Register): the panel's OoT name is the decoded one and equals
 * MM's, and the panel line (rsbs::SlotNameLine, what ComboMenuBar draws) names
 * the slot once; a half naming a different player still shows both.
 *
 *   combo-player-name (redship tier, ROM-free). The translation itself, in each
 *   of OoT's filename charsets, against explicit expected MM bytes; every byte
 *   of every charset lands on an MM glyph; every character OoT's own printer can
 *   show prints the same in MM's printer; and the panel's MM decoder.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "combo_tracker_view.h"
#include "context.h"
#include "crossing_store.h"
#include "game.h"
#include "save.h"
#include "shared_items.h"
#include "shared_resources.h"

extern "C" {
void MM_Creation_StampNewFileFields(const u8* playerName);
void OoT_PlayerName_ToMMCharset(const uint8_t* ootName, uint8_t filenameLanguage, uint8_t* mmName);
void Randomizer_TestSetOoTPlayerName(const uint8_t* name, uint8_t filenameLanguage);
void OoT_SlotMeta_DecodePlayerName(const uint8_t* blob, size_t blobSize, char outName[9]);
void OoT_SlotMeta_Register(void);
int Randomizer_TestAuthorStartedOoTBlob(uint8_t* blob, size_t blobSize, const uint8_t* name, uint8_t filenameLanguage);
void MM_SlotMeta_Register(void);
int MM_Rando_ArmCreatedHalf(int slot);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);
int OoT_Creation_AuthorRandoFile(int slot);
int Rando_HeadlessSeedTest(const char* seedStr);
void Randomizer_TestClearOoTSave(void);
void Combo_SingleBag_Forget(void);
void Combo_Crossings_Clear(void);
int MM_Combo_CaptureSaveToUnifiedSlot(void);
void MM_HarvestSharedResources(void);
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

// OoT's filename languages (soh/SaveManager.h FilenameLanguage; OoT's header is
// not includable from an MM TU).
constexpr uint8_t kLangPal = 0;
constexpr uint8_t kLangNtscJpn = 1;
constexpr uint8_t kLangNtscEng = 2;

// The name the two creation rows type on OoT's file select: NTSC English
// "Ab1 z-." and, last, a voicing mark (0xE7), which has no MM glyph.
const uint8_t kOoTName[8] = { 0xAB, 0xC6, 0x01, 0xDF, 0xDE, 0xE4, 0xEA, 0xE7 };
// ...and what MM's half must carry: A b 1 space z - . space(no glyph).
const u8 kMMName[8] = { 0x0A, 0x25, 0x01, 0x3E, 0x3D, 0x3F, 0x40, 0x3E };
// ...and what the slot panel prints (trailing space dropped).
const char* const kPanelName = "Ab1 z-.";
const u8 kMMDefaultName[8] = { 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E };

void PrintName(const char* what, const u8* name) {
    printf("[TEST] %s: %s: %02X %02X %02X %02X %02X %02X %02X %02X\n", sRow, what, name[0], name[1], name[2], name[3],
           name[4], name[5], name[6], name[7]);
}

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
    PrintName("the armed half's playerName", reinterpret_cast<const u8*>(sHalf.save.saveInfo.playerData.playerName));
    CNF_ASSERT(memcmp(sHalf.save.saveInfo.playerData.playerName, kMMName, sizeof(kMMName)) == 0,
               "the armed half carries OoT's typed name in MM's charset, not MM's all-space default (#773)");

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

    // ---- 3. the .redsave slot panel: both halves started, one name ----------
    // Both descriptors as production registers them (OoT's is registered by
    // OoT's SaveManager constructor; registering again replaces it with the
    // same descriptor). The OoT half in the slot is started, with the name the
    // creation translated: NTSC English bytes, which print as garbage unless
    // the registered OoT descriptor decodes them (#773).
    OoT_SlotMeta_Register();
    MM_SlotMeta_Register();
    const size_t ootSize = (size_t)OOT_SAVE_CONTEXT_SIZE;
    std::vector<uint8_t> ootPrev(ootSize, 0);
    memcpy(ootPrev.data(), Context_GetOoTSaveContext(), ootSize);
    {
        std::vector<uint8_t> ootStarted(ootSize, 0);
        CNF_ASSERT(Randomizer_TestAuthorStartedOoTBlob(ootStarted.data(), ootSize, kOoTName, kLangNtscEng) == 1,
                   "a started OoT half with the typed NTSC name is authored");
        Context_UpdateShadowCopy(GAME_OOT, ootStarted.data(), ootSize);
    }
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir);
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0); // an erase is what unlatches the slot for this session's write
    CNF_ASSERT(mgr.Save(0), "the slot commits");
    rsbs::SlotMeta meta = mgr.ReadMeta(0);
    const std::string nameLine = rsbs::SlotNameLine(meta);
    printf("[TEST] %s: slot meta: valid=%d ootStarted=%d mmStarted=%d ootName=\"%s\" mmName=\"%s\" line=\"%s\"\n", sRow,
           (int)meta.valid, (int)meta.ootStarted, (int)meta.mmStarted, meta.ootName, meta.mmName, nameLine.c_str());
    CNF_ASSERT(meta.valid && meta.mmStarted, "the slot panel reads the paired MM half as started");
    CNF_ASSERT(strcmp(meta.mmName, kPanelName) == 0,
               "the slot panel names MM's half by the one name, decoded from MM's charset (#773)");
    CNF_ASSERT(meta.ootStarted, "the slot panel reads the OoT half as started");
    CNF_ASSERT(strcmp(meta.ootName, kPanelName) == 0,
               "the registered OoT descriptor decodes OoT's NTSC name bytes, not the raw charset (#773)");
    CNF_ASSERT(strcmp(meta.ootName, meta.mmName) == 0, "both halves of the paired slot carry the one name");
    CNF_ASSERT(nameLine == std::string("OoT: ") + kPanelName, "the panel line names the paired slot once");
    {
        // Control: halves that really name different players still show both.
        rsbs::SlotMeta other = meta;
        snprintf(other.mmName, sizeof(other.mmName), "%s", "Zed");
        CNF_ASSERT(rsbs::SlotNameLine(other) == std::string("OoT: ") + kPanelName + "  MM: Zed",
                   "a slot whose halves name different players shows both names");
    }
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
    Context_UpdateShadowCopy(GAME_OOT, ootPrev.data(), ootSize);
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    return 0;
}

// The crash itself, as production drives it: Interface_StartMoonCrash (the
// day 4 / eventDayCount 4 / 06:00 it sets is the premise), then the cutscene's
// CS_MISC_RESET_SAVE_FROM_MOON_CRASH -> Sram_ResetSaveFromMoonCrash from the
// play state's sramCtx. The BeforeMoonCrashSaveReset hook's DeleteOwlSave
// dereferences MM_gPlayState, so a zeroed play state with a real save buffer
// stands in (its 0x19258 bytes sit in BSS), exactly as mm-moon-crash-arm-state.
int CrashTheMoon() {
    static u8 sCrashSaveBuf[SAVE_BUFFER_SIZE];
    static PlayState sCrashPlayState;
    memset(&sCrashPlayState, 0, sizeof(sCrashPlayState));
    memset(sCrashSaveBuf, 0, sizeof(sCrashSaveBuf));
    sCrashPlayState.sramCtx.saveBuf = sCrashSaveBuf;
    PlayState* const savedPlayState = MM_gPlayState;
    MM_gPlayState = &sCrashPlayState;
    Interface_StartMoonCrash(&sCrashPlayState);
    const s32 crashDay = gSaveContext.save.day;
    const s32 crashEventDay = gSaveContext.save.eventDayCount;
    Sram_ResetSaveFromMoonCrash(&sCrashPlayState.sramCtx);
    MM_gPlayState = savedPlayState;
    CNF_ASSERT(crashDay == 4 && crashEventDay == 4, "Interface_StartMoonCrash sets day 4 / eventDayCount 4 (premise)");
    return 0;
}

void PrintClock(const char* when) {
    printf("[TEST] %s: %s: day=%d eventDayCount=%d time=0x%04X rupees=%d bottle1=0x%02X isOwlSave=%d newf[0]=%02X\n",
           sRow, when, (int)gSaveContext.save.day, (int)gSaveContext.save.eventDayCount,
           (unsigned)gSaveContext.save.time, (int)gSaveContext.save.saveInfo.playerData.rupees,
           (unsigned)gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1], (int)gSaveContext.save.isOwlSave,
           (unsigned)gSaveContext.save.saveInfo.playerData.newf[0]);
}

/** The pool's rupee value (0 when never shared). */
uint16_t PoolRupees() {
    uint16_t v = 0;
    Combo_GetSharedResource(RSBS_SHARED_RES_RUPEES, &v);
    return v;
}

/** Point the SaveManager at this file's scratch directory with slot 0 armed and active. */
void OpenScratchSlot() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir);
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0); // an erase is what arms the slot for this session's writes
    RsbsSave_SetActiveSlot(0);
}

void CloseScratchSlot() {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    RsbsSave_SetActiveSlot(-1);
}

/**
 * #785: a moon crash in a paired half restores the LAST COMMIT, the file as
 * last saved, which is what vanilla's flash reload does. The arrival consumes
 * the armed half; an owl save on day 1 commits it; play goes on past that
 * commit; the moon falls.
 */
int CheckMoonCrashRestoresLastCommit(uint32_t expectSeed) {
    OpenScratchSlot();
    CNF_ASSERT(Combo_ConsumeFrozenState("mm", &gSaveContext, sizeof(gSaveContext)) == 1,
               "the armed half is consumed into the live save, as the arrival does");
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.rupeeAccumulator = 0;
    // An empty pool and no watermarks, so the rupee arithmetic below is exact
    // whatever an earlier row or the creation left behind.
    memset(gComboCtx.sharedResources, 0, sizeof(gComboCtx.sharedResources));
    memset(gComboCtx.sharedResourcesExt, 0, sizeof(gComboCtx.sharedResourcesExt));
    Combo_ResetSharedResourceWatermarks();

    // ---- the last commit: an owl save on day 1 at 10:00 --------------------
    const s32 kDay = 1;
    const s32 kEventDay = 1;
    const u16 kTime = (u16)CLOCK_TIME(10, 0);
    const s16 kRupees = 50;
    gSaveContext.save.day = kDay;
    gSaveContext.save.eventDayCount = kEventDay;
    gSaveContext.save.time = kTime;
    gSaveContext.save.saveInfo.playerData.rupees = kRupees;
    gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] = ITEM_NONE;
    gSaveContext.save.isOwlSave = true; // the owl statue's marshal state
    CNF_ASSERT(MM_Combo_CaptureSaveToUnifiedSlot() == 1, "the owl save commits the whole file (production funnel)");
    gSaveContext.save.isOwlSave = false;
    const uint32_t committedGeneration = gComboCtx.commitGeneration;
    const uint16_t committedPool = PoolRupees();
    static SharedItem sCommittedItems[RSBS_SHARED_ITEM_CAP];
    memcpy(sCommittedItems, gComboCtx.sharedItemsTagged, sizeof(sCommittedItems));
    const uint32_t committedCrossings = Combo_Crossings_Digest();
    const uint32_t committedSettingsHash = gComboCtx.comboSettingsHash;
    const size_t ootSize = (size_t)OOT_SAVE_CONTEXT_SIZE;
    std::vector<uint8_t> ootCommitted(ootSize, 0);
    memcpy(ootCommitted.data(), Context_GetOoTSaveContext(), ootSize);
    printf("[TEST] %s: committed generation %u, pool rupees %u\n", sRow, (unsigned)committedGeneration,
           (unsigned)committedPool);
    CNF_ASSERT(committedPool == (uint16_t)kRupees, "the commit harvested MM's balance into the pool");

    // ---- play on past it: nothing below is ever committed ------------------
    gSaveContext.save.day = 3;
    gSaveContext.save.eventDayCount = 3;
    gSaveContext.save.time = (u16)CLOCK_TIME(23, 0);
    gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
    gSaveContext.save.saveInfo.playerData.rupees = 200;
    MM_HarvestSharedResources(); // the pool and the watermark follow MM to 200 (a suspend does this)
    CNF_ASSERT(PoolRupees() == 200, "the uncommitted rupees reached the pool (premise)");
    CNF_ASSERT(Combo_RecordSharedItemCrossing(GAME_OOT, 0x0042) >= 0,
               "an MM check yielded an OoT item after the commit: Tier-1 records it (premise)");
    CNF_ASSERT(memcmp(sCommittedItems, gComboCtx.sharedItemsTagged, sizeof(sCommittedItems)) != 0,
               "Tier-1 moved past the commit (premise)");
    {
        // OoT's half moved past the commit too: the case where the commit
        // predates an OoT leg that was never saved.
        std::vector<uint8_t> ootLater = ootCommitted;
        ootLater[0x100] ^= 0x5A;
        Context_UpdateShadowCopy(GAME_OOT, ootLater.data(), ootSize);
    }
    PrintClock("before the crash");

    if (CrashTheMoon() != 0) {
        return 1;
    }
    PrintClock("after the moon-crash reset");
    printf("[TEST] %s: after the reset: pool rupees %u, generation %u\n", sRow, (unsigned)PoolRupees(),
           (unsigned)gComboCtx.commitGeneration);

    // ---- the vanilla outcome: the file as last saved -------------------------
    CNF_ASSERT(gSaveContext.save.day == kDay && gSaveContext.save.eventDayCount == kEventDay &&
                   gSaveContext.save.time == kTime,
               "the moon crash restores the last commit's day, eventDayCount and time (not day 4)");
    CNF_ASSERT(gSaveContext.save.saveInfo.playerData.rupees == kRupees &&
                   gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] == ITEM_NONE,
               "the moon crash restores the last commit's inventory; progress after it is lost");
    CNF_ASSERT(!gSaveContext.save.isOwlSave, "the restored owl commit is not left marked as an owl save");
    CNF_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0,
               "the half is not empty: the marker survives");
    CNF_ASSERT(gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO &&
                   gSaveContext.save.shipSaveInfo.rando.finalSeed == expectSeed,
               "the half is still this world (save type and seed)");
    CNF_ASSERT(gSaveContext.fileNum == 0xFF, "the session keeps its 0xFF sentinel");
    // ---- the whole file, not the half alone ------------------------------------
    CNF_ASSERT(memcmp(sCommittedItems, gComboCtx.sharedItemsTagged, sizeof(sCommittedItems)) == 0,
               "Tier-1's shared-item records are the commit's: the post-commit pickup is gone with its check");
    CNF_ASSERT(memcmp(Context_GetOoTSaveContext(), ootCommitted.data(), ootSize) == 0,
               "OoT's half is the commit's too (one file, one reload)");
    CNF_ASSERT(Combo_Crossings_Digest() == committedCrossings && gComboCtx.comboSettingsHash == committedSettingsHash,
               "the crossing set and the frozen rules are untouched");
    CNF_ASSERT(gComboCtx.commitGeneration == committedGeneration, "the commit generation does not move");
    // ---- the shared-resource discipline ----------------------------------------
    CNF_ASSERT(PoolRupees() == committedPool,
               "the pool is the commit's: neither the uncommitted 200 nor drained by a pre-crash watermark");
    gSaveContext.save.saveInfo.playerData.rupees = kRupees - 20;
    MM_HarvestSharedResources();
    CNF_ASSERT(PoolRupees() == (uint16_t)(committedPool - 20),
               "a spend after the reset is an ordinary delta against the restored balance");

    CloseScratchSlot();
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
    // RNG, so a second InitNewSave is a different Save): the typed name (here,
    // the name the player typed on OoT's file select, in MM's charset: #773),
    // the 'ZELDA3' marker, then the checksum over Save. Button 0's cutsceneIndex
    // is the one InitSave step the creation event does not take (see
    // MM_Creation_StampNewFileFields's header), so it is left out here too.
    CNF_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.playerName, kMMDefaultName, 8) == 0,
               "MM_Sram_InitNewSave authors MM's all-space default name");
    uint8_t typed[8];
    OoT_PlayerName_ToMMCharset(kOoTName, kLangNtscEng, typed);
    PrintName("OoT's typed name in MM's charset", typed);
    CNF_ASSERT(memcmp(typed, kMMName, sizeof(kMMName)) == 0, "OoT's typed name translates into MM's charset");
    static Save sMMPath;
    memcpy(&sMMPath, &gSaveContext.save, sizeof(Save));
    memcpy(sMMPath.saveInfo.playerData.playerName, kMMName, sizeof(kMMName));
    memcpy(sMMPath.saveInfo.playerData.newf, kNewf, sizeof(kNewf));
    sMMPath.saveInfo.checksum = Sram_CalcChecksum(&sMMPath, sizeof(Save));

    MM_Creation_StampNewFileFields(typed);
    printf("[TEST] %s: checksum stamped=0x%04X MM's path=0x%04X\n", sRow, (unsigned)gSaveContext.save.saveInfo.checksum,
           (unsigned)sMMPath.saveInfo.checksum);
    CNF_ASSERT(gSaveContext.save.saveInfo.checksum == sMMPath.saveInfo.checksum,
               "the checksum is the one MM_Sram_InitSave computes");
    CNF_ASSERT(memcmp(&gSaveContext.save, &sMMPath, sizeof(Save)) == 0,
               "the stamped Save is byte-identical to MM_Sram_InitSave's, name included (button-0 cutscene aside)");
    // Stand-in for OnFileCreate's rando block (the generation's first stamp).
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = kSeed;

    CNF_ASSERT(MM_Rando_ArmCreatedHalf(0) == 0, "the authored half arms");
    CNF_ASSERT(Context_HasFrozenState(GAME_MM) != 0, "the MM shadow is armed");
    Context_SetCurrentGame(GAME_OOT);

    if (CheckArmedHalf(kSeed) != 0) {
        return 1;
    }
    if (CheckMoonCrashRestoresLastCommit(kSeed) != 0) {
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
    // What OoT's file select writes before the creation seam (Sram_InitSave).
    Randomizer_TestSetOoTPlayerName(kOoTName, kLangNtscEng);
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
    if (CheckMoonCrashRestoresLastCommit(seed) != 0) {
        return 1;
    }

    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

/**
 * mm-moon-crash-never-saved (#785). A paired half created and never saved, then
 * a session with nothing durable at all.
 *
 * Vanilla MM writes a new file to flash when it is created (MM_Sram_InitSave),
 * so a file never saved after creation reloads AS CREATED after a crash. The
 * paired creation does the same: OoT's Sram_InitSave arms the slot, the
 * creation event arms the freshly authored MM half, and Save_SaveFile commits
 * the .redsave with it. This row takes those steps (ArmSlotOnCreate, the armed
 * half, one commit), plays three days on the arrival's consumed half without
 * saving, and lets the moon fall: the half must come back as created.
 */
extern "C" int MM_MoonCrashNeverSaved_Run(void) {
    sRow = "mm-moon-crash-never-saved";
    printf("[TEST] %s: a paired half created and never saved restores as created after a moon crash; a session "
           "with no commit at all restarts the clock at dawn (#785)\n",
           sRow);

    const GameId prevGame = Context_GetCurrentGame();
    Context_InitFrozenStates();
    ComboContext_Init();
    Context_ClearAllFrozenStates();

    // ---- the creation: the synthetic row's authoring, then its one commit ----
    const uint32_t kSeed = 0x5EED0785u;
    memset(&gSaveContext, 0, sizeof(SaveContext));
    MM_Sram_InitNewSave();
    MM_Creation_StampNewFileFields(kMMName);
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = kSeed;
    CNF_ASSERT(MM_Rando_ArmCreatedHalf(0) == 0, "the authored half arms");
    Context_SetCurrentGame(GAME_OOT);
    static SaveContext sCreated;
    memcpy(&sCreated, Context_GetMMSaveContext(), sizeof(SaveContext));
    {
        // OoT's half at creation: a zeroed stand-in (the row is ROM-free).
        std::vector<uint8_t> oot((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
        Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    }
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir);
    mgr.DeleteSave(0); // leftovers from an earlier run
    mgr.ResetSlotSessionState();
    RsbsSave_ArmSlotOnCreate(0); // OoT's Sram_InitSave
    CNF_ASSERT(mgr.Save(0), "the creation commits the .redsave with the armed MM half (Save_SaveFile)");
    RsbsSave_SetActiveSlot(0);
    printf("[TEST] %s: created half: day=%d eventDayCount=%d time=0x%04X rupees=%d\n", sRow, (int)sCreated.save.day,
           (int)sCreated.save.eventDayCount, (unsigned)sCreated.save.time,
           (int)sCreated.save.saveInfo.playerData.rupees);

    // ---- the arrival, and three days of play that are never saved ------------
    CNF_ASSERT(Combo_ConsumeFrozenState("mm", &gSaveContext, sizeof(gSaveContext)) == 1,
               "the arrival consumes the armed half");
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.day = 2;
    gSaveContext.save.eventDayCount = 2;
    gSaveContext.save.time = (u16)CLOCK_TIME(15, 0);
    gSaveContext.save.saveInfo.playerData.rupees = 99;
    gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
    if (CrashTheMoon() != 0) {
        return 1;
    }
    PrintClock("after the moon crash (never saved)");
    CNF_ASSERT(gSaveContext.save.day == 0 && gSaveContext.save.eventDayCount == 0 &&
                   gSaveContext.save.time == (u16)(CLOCK_TIME(6, 0) - 1),
               "a half never saved after creation restores to the created clock: day 0, 05:59, the Dawn of the "
               "First Day");
    CNF_ASSERT(memcmp(&gSaveContext.save.saveInfo, &sCreated.save.saveInfo, sizeof(sCreated.save.saveInfo)) == 0,
               "a half never saved after creation restores to the created half: inventory, flags and name");
    CNF_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0 &&
                   memcmp(gSaveContext.save.saveInfo.playerData.playerName, kMMName, sizeof(kMMName)) == 0,
               "the half is not empty: marker and name");
    CNF_ASSERT(gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO &&
                   gSaveContext.save.shipSaveInfo.rando.finalSeed == kSeed,
               "the half is still this world");

    // ---- a session with nothing durable: no active slot, then a latched one --
    for (int leg = 0; leg < 2; leg++) {
        if (leg == 0) {
            RsbsSave_SetActiveSlot(-1);
        } else {
            mgr.ResetSlotSessionState(); // slot 0 exists but this session never loaded, created or erased it
            RsbsSave_SetActiveSlot(0);
        }
        gSaveContext.save.day = 3;
        gSaveContext.save.eventDayCount = 3;
        gSaveContext.save.time = (u16)CLOCK_TIME(23, 0);
        gSaveContext.save.saveInfo.playerData.rupees = 77;
        if (CrashTheMoon() != 0) {
            return 1;
        }
        PrintClock(leg == 0 ? "after the moon crash (no active slot)" : "after the moon crash (latched slot)");
        CNF_ASSERT(gSaveContext.save.day == 0 && gSaveContext.save.eventDayCount == 0 &&
                       gSaveContext.save.time == (u16)(CLOCK_TIME(6, 0) - 1),
                   "with no commit to restore, the clock alone restarts at dawn instead of staying on day 4");
        CNF_ASSERT(gSaveContext.save.saveInfo.playerData.rupees == 77 &&
                       memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0,
                   "with no commit to restore, nothing is reloaded and the half is not emptied");
    }

    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    RsbsSave_SetActiveSlot(-1);
    Context_ClearAllFrozenStates();
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

namespace {

/** What OoT's own name printer shows for one name byte (z_message_PAL.c's
 *  MESSAGE_NAME decode, non-JPN text): '?' for a byte it shows as a blank or a
 *  kana glyph MM cannot draw, which the translation must turn into MM's space. */
char OoTPrints(uint8_t c, uint8_t lang) {
    if (lang == kLangPal) {
        if (c == 0x3E) {
            return ' ';
        } else if (c == 0x40) {
            return '.';
        } else if (c == 0x3F) {
            return '-';
        } else if (c < 0x0A) {
            return (char)('0' + c);
        } else if (c < 0x24) {
            return (char)(c + '7'); // 0x0A + '7' == 'A'
        } else if (c < 0x3E) {
            return (char)(c + '='); // 0x24 + '=' == 'a'
        }
        return '?';
    }
    if (c == 0xDF) {
        return ' ';
    } else if (c == 0xEA) {
        return '.';
    } else if (c == 0xE4) {
        return '-';
    } else if (c < 0x0A) {
        return (char)('0' + c);
    } else if (c >= 0xAB && c < 0xC5) {
        return (char)(c - 0x6A);
    } else if (c >= 0xC5 && c < 0xDF) {
        return (char)(c - 0x64);
    }
    return '?';
}

/** What MM's name printer shows for one byte (z_message_nes.c MESSAGE_NAME). */
char MMPrints(uint8_t c) {
    if (c == 0x3E) {
        return ' ';
    } else if (c == 0x40) {
        return '.';
    } else if (c == 0x3F) {
        return '-';
    } else if (c < 0x0A) {
        return (char)('0' + c);
    } else if (c < 0x24) {
        return (char)('A' - 10 + c);
    } else if (c < 0x3E) {
        return (char)('a' - 36 + c);
    }
    return '?';
}

struct NameCase {
    const char* what;
    uint8_t lang;
    uint8_t oot[8];
    uint8_t mm[8];
};

// Expected MM bytes, written out by hand from the two charsets (never computed
// by the code under test).
const NameCase kCases[] = {
    // "Link" typed on NTSC English: MM's "LINK" default is 15 12 17 14.
    { "NTSC English \"Link\"",
      kLangNtscEng,
      { 0xB6, 0xCD, 0xD2, 0xCF, 0xDF, 0xDF, 0xDF, 0xDF },
      { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E } },
    // Letters at both ends of both cases, digits 0 and 9, space, '-', '.'.
    { "NTSC English \"Zaz09-. \"",
      kLangNtscEng,
      { 0xC4, 0xC5, 0xDE, 0x00, 0x09, 0xE4, 0xEA, 0xDF },
      { 0x23, 0x24, 0x3D, 0x00, 0x09, 0x3F, 0x40, 0x3E } },
    // The creation rows' name, with the voicing mark (0xE7: no MM glyph).
    { "NTSC English \"Ab1 z-.\" + voicing mark",
      kLangNtscEng,
      { 0xAB, 0xC6, 0x01, 0xDF, 0xDE, 0xE4, 0xEA, 0xE7 },
      { 0x0A, 0x25, 0x01, 0x3E, 0x3D, 0x3F, 0x40, 0x3E } },
    // NTSC Japanese: hiragana A (0x0A) and katakana A (0x5A) have no MM glyph;
    // the alphanumeric page's 'A', the long-vowel mark (same byte as '-'), both
    // voicing marks, a digit, space.
    { "NTSC Japanese kana + Latin",
      kLangNtscJpn,
      { 0x0A, 0x5A, 0xAB, 0xE4, 0xE7, 0xE8, 0x09, 0xDF },
      { 0x3E, 0x3E, 0x0A, 0x3F, 0x3E, 0x3E, 0x09, 0x3E } },
    // PAL: the PAL charset is MM's, so every PAL name byte carries over as is;
    // 0x41 and 0xDF are not PAL name bytes and become MM's space.
    { "PAL \"Aa0 -.\" + two non-PAL bytes",
      kLangPal,
      { 0x0A, 0x24, 0x00, 0x3E, 0x3F, 0x40, 0x41, 0xDF },
      { 0x0A, 0x24, 0x00, 0x3E, 0x3F, 0x40, 0x3E, 0x3E } },
};

} // namespace

extern "C" int MM_CreationPlayerName_Run(void) {
    sRow = "combo-player-name";
    printf("[TEST] %s: OoT's typed name becomes the paired MM half's name in MM's charset (#773)\n", sRow);

    // ---- 1. hand-written cases ----------------------------------------------
    for (const NameCase& c : kCases) {
        uint8_t mm[8];
        memset(mm, 0xAA, sizeof(mm));
        OoT_PlayerName_ToMMCharset(c.oot, c.lang, mm);
        printf("[TEST] %s: %s -> %02X %02X %02X %02X %02X %02X %02X %02X\n", sRow, c.what, mm[0], mm[1], mm[2], mm[3],
               mm[4], mm[5], mm[6], mm[7]);
        CNF_ASSERT(memcmp(mm, c.mm, sizeof(mm)) == 0, c.what);
    }

    // ---- 2. total: every byte of every charset lands on an MM glyph ---------
    // (0x00-0x40, the bytes MM's printer and MM's font draw). An out-of-range
    // language (7) reads as NTSC, as SoH's own name printer reads it.
    const uint8_t langs[] = { kLangPal, kLangNtscJpn, kLangNtscEng, 7 };
    for (uint8_t lang : langs) {
        for (int b = 0; b < 256; b++) {
            const uint8_t in[8] = { (uint8_t)b, (uint8_t)b, (uint8_t)b, (uint8_t)b,
                                    (uint8_t)b, (uint8_t)b, (uint8_t)b, (uint8_t)b };
            uint8_t mm[8];
            OoT_PlayerName_ToMMCharset(in, lang, mm);
            for (int i = 0; i < 8; i++) {
                if (mm[i] > 0x40 || MMPrints(mm[i]) == '?') {
                    printf("[TEST] %s: lang %u byte 0x%02X -> 0x%02X\n", sRow, (unsigned)lang, (unsigned)b,
                           (unsigned)mm[i]);
                    CNF_ASSERT(false, "every OoT name byte lands on a byte MM can print");
                }
            }
            // ---- 3. what OoT prints, MM prints ------------------------------
            // A character OoT's printer shows (a digit, a letter, space, '-',
            // '.') prints as the same character in MM; anything else becomes
            // MM's space.
            const char want = OoTPrints((uint8_t)b, lang == kLangPal ? kLangPal : kLangNtscEng);
            const bool kana = lang == kLangNtscJpn && ((b >= 0x0A && b < 0xAB) || b == 0xE7 || b == 0xE8);
            const char expect = (want == '?' || kana) ? ' ' : want;
            if (MMPrints(mm[0]) != expect) {
                printf("[TEST] %s: lang %u byte 0x%02X: OoT prints '%c', MM prints '%c'\n", sRow, (unsigned)lang,
                       (unsigned)b, expect, MMPrints(mm[0]));
                CNF_ASSERT(false, "a character OoT prints, MM prints the same (no-glyph bytes: MM's space)");
            }
        }
    }
    // Every typable NTSC English character is covered by (3); count them so a
    // drift in either printer table cannot silently empty the check.
    int typable = 0;
    for (int b = 0; b < 256; b++) {
        typable += OoTPrints((uint8_t)b, kLangNtscEng) != '?' ? 1 : 0;
    }
    printf("[TEST] %s: NTSC English printable name bytes checked: %d (10 digits + 52 letters + space, '-', '.')\n",
           sRow, typable);
    CNF_ASSERT(typable == 65, "OoT's NTSC printer shows 65 distinct name characters");

    // ---- 4. the slot panel's decoder ----------------------------------------
    char out[9];
    RsbsSave_DecodeN64FilenameName(kCases[0].mm, out);
    printf("[TEST] %s: panel decode of MM \"Link\" = \"%s\"\n", sRow, out);
    CNF_ASSERT(strcmp(out, "Link") == 0, "the panel prints MM's \"Link\" as Link (trailing spaces dropped)");
    RsbsSave_DecodeN64FilenameName(kMMName, out);
    CNF_ASSERT(strcmp(out, kPanelName) == 0, "the panel prints the creation rows' name");
    RsbsSave_DecodeN64FilenameName(kMMDefaultName, out);
    CNF_ASSERT(out[0] == '\0', "MM's all-space default (a half created before #773) prints as no name");
    const uint8_t offCharset[8] = { 0x15, 0x41, 0xDF, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E };
    RsbsSave_DecodeN64FilenameName(offCharset, out);
    CNF_ASSERT(strcmp(out, "L??") == 0, "a byte outside MM's charset prints as '?'");

    // ---- 5. the slot panel's OoT decoder, over a synthetic OoT save ---------
    // The panel copied OoT's raw charset bytes before #773; it now prints them
    // through the same translation, in the file's own filename language.
    struct OoTPanelCase {
        uint8_t lang;
        uint8_t oot[8];
        const char* expect;
    };
    const OoTPanelCase panel[] = {
        { kLangNtscEng, { 0xAB, 0xC6, 0x01, 0xDF, 0xDE, 0xE4, 0xEA, 0xE7 }, "Ab1 z-." },
        { kLangNtscEng, { 0xB6, 0xCD, 0xD2, 0xCF, 0xDF, 0xDF, 0xDF, 0xDF }, "Link" },
        { kLangPal, { 0x15, 0x12, 0x17, 0x14, 0x3E, 0x3E, 0x3E, 0x3E }, "LINK" },
        { kLangNtscJpn, { 0x0A, 0x5A, 0xAB, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF }, "  A" },
    };
    for (const OoTPanelCase& p : panel) {
        Randomizer_TestClearOoTSave();
        Randomizer_TestSetOoTPlayerName(p.oot, p.lang);
        OoT_SlotMeta_DecodePlayerName(reinterpret_cast<const uint8_t*>(&gSaveContext), (size_t)OOT_SAVE_CONTEXT_SIZE,
                                      out);
        printf("[TEST] %s: OoT panel decode (language %u) = \"%s\"\n", sRow, (unsigned)p.lang, out);
        CNF_ASSERT(strcmp(out, p.expect) == 0, "the slot panel prints OoT's name in the file's own charset");
    }
    Randomizer_TestClearOoTSave();

    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}
