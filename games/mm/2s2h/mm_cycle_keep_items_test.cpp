/**
 * @file mm_cycle_keep_items_test.cpp
 * Majora's Mask's "Keep Items on Cycle Reset" (#1175; OoTMM's keepItemsReset).
 *
 *   mm-cycle-keep-items (redship tier, ROM-free). Three legs.
 *     (0) THE #1175 REPRO: a Powder Keg found in Hyrule is redeemed in Termina
 *         the way every crossing is (Combo_RecordSharedItemCrossing, MM's real
 *         arrival consumer MM_ConsumeSharedItems, the real first-frame drain
 *         MM_ForeignItem_FlushPending over a stand-in PlayState, which gives
 *         through Rando::GiveItem -> MM_Item_Give): the slot AND a count of 1,
 *         and again after one suspend/resume round trip through MM's real
 *         shared-resource shims. Green before and after the fix: the count is
 *         lost only by the three-day reset, which is what the option is for.
 *     (1) Option ON (the frozen gComboCtx.mmKeepItemsReset): a headless
 *         Sram_SaveEndOfCycle keeps the keg (1) and the Magic Beans, wipes the
 *         pictograph, and keeps the five shared counts and the rupees with every
 *         "lost" flag clear.
 *     (2) Option OFF (vanilla): the keg's slot stays and its count is 0, the
 *         beans are 0; the shared counts and rupees are kept as in (1).
 *
 *   mm-keep-items-reset-frozen (redship tier, ROM-free; needs the CVar store).
 *     (1) Identity: the key off adds nothing to MM's profile stamp (no golden
 *         moves), on moves it.
 *     (2) The creation freeze writes the byte from the key, and play reads the
 *         byte, never the key.
 *     (3) The byte is KEPT through the file-create invalidation, DROPPED by any
 *         other.
 *     (4) A seed package's world record carries it (WorldRecordBytes ->
 *         ApplyComboWorld).
 *     (5) A .redsave carries it (Tier-1 Save/Load).
 *     (6) The load-time restore: a file frozen ON loaded with the key off is
 *         RESTORABLE and the restore writes the key back.
 *
 *   mm-cycle-razor-sword (redship tier, ROM-free; #1215). The rando's own
 *   cycle-save path keeps the seed's Razor Sword through the three-day reset.
 *   Rando is armed through the production path (saveType SAVETYPE_RANDO, then
 *   what OnSaveLoadHandler runs on every MM load: Rando::MiscBehavior::OnFileLoad
 *   and ShipInit::Init("IS_RANDO"), which arms EndOfCycle.cpp's Razor restore,
 *   `CVAR || IS_RANDO`), and the reset is the same headless Sram_SaveEndOfCycle
 *   as above.
 *     (a) Razor on B, swordHealth 60: still the Razor after the Song of Time
 *         (vanilla's wipe writes the Kokiri Sword over it).
 *     (b) No sword, Takkuri holding the Razor (STOLEN_ITEM_1): the Razor back.
 *     (c) No sword and nothing stolen: still no sword (the rando's existing
 *         no-sword override, kept).
 *     (d) Control: a vanilla save holding the Razor drops to the Kokiri Sword.
 *   It names nothing 2s2h/Enhancements/Cycle/EndOfCycle.cpp defines (W6-
 *   CYCLE-CHEATS links it whole and retired #1215's inline port in
 *   OnCycleSave.cpp): a reference from here would link it by itself, so a
 *   dropped link would read green.
 *
 * Every CVar a leg sets is cleared before the row returns (the gate-config
 * contamination class); the .redsave leg writes only the per-process scratch
 * directory.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <vector>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The combo surfaces are single-exe only; test_runner.cpp declares these
// unconditionally, so a standalone 2ship build links.
extern "C" int MM_CycleKeepItems_Run(void) {
    return 1;
}
extern "C" int MM_KeepItemsResetFrozen_Run(void) {
    return 1;
}
extern "C" int MM_CycleRazorSword_Run(void) {
    return 1;
}
#else

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/ShipInit.hpp"
#include "combo_mm_options_view.h"
#include "context.h"
#include "save.h"
#include "seed_package.h"
#include "shared_items.h"
#include "test_scratch.h" // a scratch directory per process

extern "C" {
// GameExports_SingleExe.cpp: the #525 cycle bracket's registrar (production
// registers it once from MM_Rando_InitCore, which a ROM-free row never runs),
// MM's arrival consumer and its two shared-resource shims.
void MM_RegisterSharedResourceCycleHooks(void);
void MM_ConsumeSharedItems(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
// Rando/ForeignItemsSingleExe.cpp: the deferred give's queue.
int MM_ForeignItem_FlushPending(void);
int MM_ForeignItem_TestPendingCount(void);
void MM_ForeignItem_TestResetPending(void);
}

namespace {

const char* sRow = "mm-cycle-keep-items";

#define KIR_ASSERT(cond, msg)                                                                     \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            printf("[TEST] FAIL (%s): %s  [%s] (%s:%d)\n", sRow, msg, #cond, __FILE__, __LINE__); \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

inline const char* kSaveDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_saves_cycle_keep_items");
}

// The counts each cycle leg starts from: the five shared counts, the rupees
// (under the tier-0 wallet's 99), and the two MM-only counts.
constexpr s8 kArrows = 20;
constexpr s8 kBombs = 10;
constexpr s8 kBombchus = 5;
constexpr s8 kSticks = 7;
constexpr s8 kNuts = 9;
constexpr s16 kRupees = 77;
constexpr s8 kKeg = 1;
constexpr s8 kBeans = 5;

/** MM's live save as a cross-game MM session runs it (the 0xFF "no flash slot"
 *  sentinel, GAMEMODE_NORMAL, which the shims' Combo_SaveIsLiveFile gate needs),
 *  every item slot ITEM_NONE as a real save's, human form. */
void ArmLiveSave() {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
}

/** The inventory a cycle leg plays the Song of Time with. */
void ArmCycleInventory() {
    ArmLiveSave();
    MM_Inventory_ChangeUpgrade(UPG_QUIVER, 1);
    MM_Inventory_ChangeUpgrade(UPG_BOMB_BAG, 1);
    MM_Inventory_ChangeUpgrade(UPG_DEKU_STICKS, 1);
    MM_Inventory_ChangeUpgrade(UPG_DEKU_NUTS, 1);
    INV_CONTENT(ITEM_BOW) = ITEM_BOW;
    INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
    INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
    INV_CONTENT(ITEM_DEKU_STICK) = ITEM_DEKU_STICK;
    INV_CONTENT(ITEM_DEKU_NUT) = ITEM_DEKU_NUT;
    INV_CONTENT(ITEM_MAGIC_BEANS) = ITEM_MAGIC_BEANS;
    INV_CONTENT(ITEM_POWDER_KEG) = ITEM_POWDER_KEG;
    INV_CONTENT(ITEM_PICTOGRAPH_BOX) = ITEM_PICTOGRAPH_BOX;
    AMMO(ITEM_BOW) = kArrows;
    AMMO(ITEM_BOMB) = kBombs;
    AMMO(ITEM_BOMBCHU) = kBombchus;
    AMMO(ITEM_DEKU_STICK) = kSticks;
    AMMO(ITEM_DEKU_NUT) = kNuts;
    AMMO(ITEM_MAGIC_BEANS) = kBeans;
    AMMO(ITEM_POWDER_KEG) = kKeg;
    AMMO(ITEM_PICTOGRAPH_BOX) = 0;
    SET_QUEST_ITEM(QUEST_PICTOGRAPH);
    gSaveContext.save.saveInfo.playerData.rupees = kRupees;
    gSaveContext.save.saveInfo.playerData.health = 0x30;
}

/** The cycle bracket's hooks, registered once per process (re-registering is
 *  harmless, both halves are idempotent, but one is what production has). */
void RegisterCycleHooksOnce() {
    static bool sRegistered = false;
    if (!sRegistered) {
        MM_RegisterSharedResourceCycleHooks();
        sRegistered = true;
    }
}

/** The Song of Time's reset, headless: the REAL Sram_SaveEndOfCycle over a
 *  zeroed stand-in PlayState (as the moon-crash rows run it), bracketed by the
 *  REAL Before/AfterEndOfCycleSave dispatch it calls. */
void PlaySongOfTime() {
    static PlayState sStandIn;
    memset(&sStandIn, 0, sizeof(sStandIn));
    PlayState* const previous = MM_gPlayState;
    MM_gPlayState = &sStandIn;
    Sram_SaveEndOfCycle(&sStandIn);
    MM_gPlayState = previous;
}

void PrintCounts(const char* when) {
    printf("[TEST] %s: %s: keg slot 0x%02X count %d, beans %d, pictograph quest %d ammo %d; arrows %d bombs %d "
           "bombchus %d sticks %d nuts %d rupees %d; eventInf[7] %02X\n",
           sRow, when, (unsigned)INV_CONTENT(ITEM_POWDER_KEG), (int)AMMO(ITEM_POWDER_KEG), (int)AMMO(ITEM_MAGIC_BEANS),
           CHECK_QUEST_ITEM(QUEST_PICTOGRAPH) ? 1 : 0, (int)AMMO(ITEM_PICTOGRAPH_BOX), (int)AMMO(ITEM_BOW),
           (int)AMMO(ITEM_BOMB), (int)AMMO(ITEM_BOMBCHU), (int)AMMO(ITEM_DEKU_STICK), (int)AMMO(ITEM_DEKU_NUT),
           (int)gSaveContext.save.saveInfo.playerData.rupees, (unsigned)gSaveContext.eventInf[7]);
}

/** What both cycle legs require: the five shared counts and the rupees kept
 *  (#525), every "lost" flag clear, the pictograph wiped. 0 when it holds. */
int CheckSharedKept(const char* leg) {
    if (AMMO(ITEM_BOW) != kArrows || AMMO(ITEM_BOMB) != kBombs || AMMO(ITEM_BOMBCHU) != kBombchus ||
        AMMO(ITEM_DEKU_STICK) != kSticks || AMMO(ITEM_DEKU_NUT) != kNuts) {
        printf("[TEST] FAIL (%s): %s: a shared ammo count did not survive the reset (#525's bracket)\n", sRow, leg);
        return 1;
    }
    if (gSaveContext.save.saveInfo.playerData.rupees != kRupees) {
        printf("[TEST] FAIL (%s): %s: the shared rupees did not survive the reset (#525's bracket)\n", sRow, leg);
        return 1;
    }
    if (CHECK_EVENTINF(EVENTINF_THREEDAYRESET_LOST_RUPEES) || CHECK_EVENTINF(EVENTINF_THREEDAYRESET_LOST_BOMB_AMMO) ||
        CHECK_EVENTINF(EVENTINF_THREEDAYRESET_LOST_NUT_AMMO) ||
        CHECK_EVENTINF(EVENTINF_THREEDAYRESET_LOST_STICK_AMMO) ||
        CHECK_EVENTINF(EVENTINF_THREEDAYRESET_LOST_ARROW_AMMO)) {
        printf("[TEST] FAIL (%s): %s: a \"you lost it\" flag is set for something the reset did not take\n", sRow, leg);
        return 1;
    }
    if (CHECK_QUEST_ITEM(QUEST_PICTOGRAPH) || AMMO(ITEM_PICTOGRAPH_BOX) != 0) {
        printf("[TEST] FAIL (%s): %s: the pictograph survived the reset (OoTMM resets it under both values)\n", sRow,
               leg);
        return 1;
    }
    return 0;
}

} // namespace

extern "C" int MM_CycleKeepItems_Run(void) {
    sRow = "mm-cycle-keep-items";
    printf("[TEST] %s: a Powder Keg crossing arrives as one keg; Keep Items on Cycle Reset keeps it and the Magic "
           "Beans through the three-day reset, off wipes them as vanilla (#1175)\n",
           sRow);
    ComboContext_Init();
    RegisterCycleHooksOnce();

    // ---- (0) the #1175 repro: the crossing's redemption ---------------------
    ArmLiveSave();
    Combo_ClearSharedItemOutbox();
    MM_ForeignItem_TestResetPending();
    KIR_ASSERT(Combo_RecordSharedItemCrossing(GAME_MM, (uint16_t)RI_POWDER_KEG) >= 0,
               "(0) the keg's crossing was not recorded");
    MM_ConsumeSharedItems();
    KIR_ASSERT(Combo_CountSharedItems(GAME_MM, /*includeRedeemed=*/false) == 0,
               "(0) the arrival did not redeem the keg's crossing");
    KIR_ASSERT(MM_ForeignItem_TestPendingCount() == 1, "(0) the keg's give is not waiting for the first frame");
    {
        static PlayState sStandIn;
        memset(&sStandIn, 0, sizeof(sStandIn));
        PlayState* const previous = MM_gPlayState;
        MM_gPlayState = &sStandIn;
        const int given = MM_ForeignItem_FlushPending();
        MM_gPlayState = previous;
        KIR_ASSERT(given == 1, "(0) the first gameplay frame's drain did not give the keg");
    }
    PrintCounts("(0) after the crossing's first-frame give");
    KIR_ASSERT(INV_CONTENT(ITEM_POWDER_KEG) == ITEM_POWDER_KEG && AMMO(ITEM_POWDER_KEG) == 1,
               "(0) #1175 AT ARRIVAL: the keg crossing gave the slot without one keg (a redemption defect)");
    MM_HarvestSharedResources(); // the suspend
    MM_ApplySharedResources();   // the next arrival
    PrintCounts("(0) after one suspend/resume round trip");
    KIR_ASSERT(INV_CONTENT(ITEM_POWDER_KEG) == ITEM_POWDER_KEG && AMMO(ITEM_POWDER_KEG) == 1,
               "(0) the shared-resource shims touched the keg (it is no shared kind)");
    printf("[TEST] %s: (0) the keg arrives as one keg and survives a round trip: #1175's 0 is the cycle reset\n", sRow);

    // ---- (1) option ON -------------------------------------------------------
    ComboContext_Init();
    gComboCtx.mmKeepItemsReset = 1;
    ArmCycleInventory();
    PrintCounts("(1) option on, before the Song of Time");
    PlaySongOfTime();
    PrintCounts("(1) option on, after the Song of Time");
    KIR_ASSERT(INV_CONTENT(ITEM_POWDER_KEG) == ITEM_POWDER_KEG && AMMO(ITEM_POWDER_KEG) == kKeg,
               "(1) Keep Items on Cycle Reset is on and the reset took the Powder Keg");
    KIR_ASSERT(AMMO(ITEM_MAGIC_BEANS) == kBeans, "(1) Keep Items on Cycle Reset is on and the reset took the beans");
    if (CheckSharedKept("(1) option on") != 0) {
        return 1;
    }

    // ---- (2) option OFF (vanilla) --------------------------------------------
    ComboContext_Init();
    ArmCycleInventory();
    PlaySongOfTime();
    PrintCounts("(2) option off, after the Song of Time");
    KIR_ASSERT(INV_CONTENT(ITEM_POWDER_KEG) == ITEM_POWDER_KEG && AMMO(ITEM_POWDER_KEG) == 0,
               "(2) with the option off the reset must take the keg and keep its slot, as vanilla does");
    KIR_ASSERT(AMMO(ITEM_MAGIC_BEANS) == 0, "(2) with the option off the reset must take the beans, as vanilla does");
    if (CheckSharedKept("(2) option off") != 0) {
        return 1;
    }

    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

extern "C" int MM_KeepItemsResetFrozen_Run(void) {
    sRow = "mm-keep-items-reset-frozen";
    printf("[TEST] %s: Keep Items on Cycle Reset freezes at creation, is world identity only when on, and travels "
           "with the world (file-create, seed package, .redsave, load-time restore) (#1175)\n",
           sRow);
    CVarClear(RSBS_CVAR_MM_KEEP_ITEMS_RESET);
    int failures = 0;
    auto check = [&failures](bool ok, const char* what) {
        if (!ok) {
            failures++;
            printf("[TEST] FAIL (%s): %s\n", sRow, what);
        }
    };

    // ---- (1) identity ---------------------------------------------------------
    const uint32_t stampDefault = MM_Rando_ComputeProfileStamp();
    CVarSetInteger(RSBS_CVAR_MM_KEEP_ITEMS_RESET, 0);
    const uint32_t stampOff = MM_Rando_ComputeProfileStamp();
    CVarSetInteger(RSBS_CVAR_MM_KEEP_ITEMS_RESET, 1);
    const uint32_t stampOn = MM_Rando_ComputeProfileStamp();
    printf("[TEST] %s: (1) profile stamp: key unset %08X, off %08X, on %08X (expected equal, equal, different)\n", sRow,
           (unsigned)stampDefault, (unsigned)stampOff, (unsigned)stampOn);
    check(stampOff == stampDefault, "(1) the option OFF moved MM's profile stamp: every existing pair would refuse");
    check(stampOn != stampDefault,
          "(1) the option ON is not part of MM's identity: a change after creation would go unnoticed");

    // ---- (2) the creation freeze ---------------------------------------------
    ComboContext_Init();
    MM_Rando_FreezeKeepItemsResetAtCreation(); // key on
    const unsigned frozenOn = gComboCtx.mmKeepItemsReset;
    CVarClear(RSBS_CVAR_MM_KEEP_ITEMS_RESET);
    const int playsOn = MM_Rando_FrozenKeepItemsReset();
    printf("[TEST] %s: (2) frozen with the key on: byte %u, plays as %d with the key since cleared (expected 1, 1)\n",
           sRow, frozenOn, playsOn);
    check(frozenOn == 1u, "(2) the creation freeze did not write the authored option into the combo record");
    check(playsOn == 1, "(2) the file does not play under its frozen option (a key read, or no read at all)");
    MM_Rando_FreezeKeepItemsResetAtCreation(); // key cleared: off
    check(gComboCtx.mmKeepItemsReset == 0 && MM_Rando_FrozenKeepItemsReset() == 0,
          "(2) the creation freeze with the key off left the option on");

    // ---- (3) the file-create invalidation KEEPs it, any other DROPs it ------
    ComboContext_Init();
    gComboCtx.mmKeepItemsReset = 1;
    Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
    const unsigned kept = gComboCtx.mmKeepItemsReset;
    Context_InvalidateSessionState(RSBS_SEED_STAMP_DROP);
    const unsigned dropped = gComboCtx.mmKeepItemsReset;
    printf("[TEST] %s: (3) after the file-create invalidation %u, after a drop %u (expected 1, 0)\n", sRow, kept,
           dropped);
    check(kept == 1u, "(3) creating the file wiped the option the creation event froze for it");
    check(dropped == 0u, "(3) a session drop kept a dead world's option");

    // ---- (4) a seed package's world record -----------------------------------
    ComboContext_Init();
    gComboCtx.mmKeepItemsReset = 1;
    OoT_SeedPackage_TestReapplyWorldRecord();
    printf("[TEST] %s: (4) through the package's world record: %u (expected 1)\n", sRow,
           (unsigned)gComboCtx.mmKeepItemsReset);
    check(gComboCtx.mmKeepItemsReset == 1, "(4) a seed package drops the option: a file made from it plays vanilla");

    // ---- (5) a .redsave (Tier-1) ---------------------------------------------
    {
        Context_InitFrozenStates();
        ComboContext_Init();
        gComboCtx.mmKeepItemsReset = 1;
        std::vector<uint8_t> oot((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
        std::vector<uint8_t> mm((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
        Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.SetSaveDirectory(kSaveDir());
        mgr.DeleteSave(0);
        const bool saved = mgr.Save(0);
        ComboContext_Init();
        const bool loaded = saved && mgr.Load(0);
        printf("[TEST] %s: (5) .redsave save %d, load %d, byte %u (expected 1, 1, 1)\n", sRow, saved ? 1 : 0,
               loaded ? 1 : 0, (unsigned)gComboCtx.mmKeepItemsReset);
        check(saved && loaded && gComboCtx.mmKeepItemsReset == 1, "(5) the .redsave does not carry the option");
        mgr.DeleteSave(0);
        mgr.ResetSlotSessionState();
        mgr.SetSaveDirectory("Save");
        Context_ClearAllFrozenStates();
    }

    // ---- (6) the load-time restore -------------------------------------------
    {
        ComboContext_Init();
        CVarSetInteger(RSBS_CVAR_MM_KEEP_ITEMS_RESET, 1);
        const uint32_t frozen = MM_Rando_ComputeProfileStamp();
        // The file's MM half, authored as the creation event authors it: the
        // stamp first, then the PAIRED resolution (its logic pin is part of the
        // identity), which compares against that stamp.
        gComboCtx.mmProfileDigest = frozen;
        memset(&gSaveContext, 0, sizeof(SaveContext));
        MM_Sram_InitNewSave();
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        Rando::Foreign::ResolvePairedProfile(/*paired=*/true);
        std::vector<uint8_t> half(sizeof(SaveContext));
        memcpy(half.data(), &gSaveContext, sizeof(SaveContext));
        CVarClear(RSBS_CVAR_MM_KEEP_ITEMS_RESET); // the player turned it off at the title
        const int classified = MM_Rando_ClassifyProfileForLoad(half.data(), half.size(), frozen);
        char names[256] = { 0 };
        int count = 0;
        const int restored =
            MM_Rando_RestoreProfileForLoad(half.data(), half.size(), frozen, names, sizeof(names), &count);
        const int keyAfter = CVarGetInteger(RSBS_CVAR_MM_KEEP_ITEMS_RESET, 0);
        const uint32_t after = MM_Rando_ComputeProfileStamp();
        printf("[TEST] %s: (6) frozen ON, key off: classified %d, restore %d (%d key(s): %s), key now %d, stamp %08X "
               "vs frozen %08X (expected %d, %d, 1, equal)\n",
               sRow, classified, restored, count, names, keyAfter, (unsigned)after, (unsigned)frozen,
               RSBS_MM_PROFILE_LOAD_RESTORABLE, RSBS_MM_PROFILE_LOAD_RESTORED);
        check(classified == RSBS_MM_PROFILE_LOAD_RESTORABLE,
              "(6) a file frozen with the option on does not classify as restorable with the key off");
        check(restored == RSBS_MM_PROFILE_LOAD_RESTORED && keyAfter == 1 && after == frozen,
              "(6) the load did not put the file's option back into its key");
        CVarClear(RSBS_CVAR_MM_KEEP_ITEMS_RESET);
    }

    CVarClear(RSBS_CVAR_MM_KEEP_ITEMS_RESET);
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    if (failures != 0) {
        printf("[TEST] FAIL: %s (%d failure(s))\n", sRow, failures);
        return 1;
    }
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

namespace {

/** The sword the reset left: equip value and the human B button. */
void PrintSword(const char* when) {
    printf("[TEST] %s: %s: sword equip value %u, human B 0x%02X, swordHealth %u, stolen 0x%08X\n", sRow, when,
           (unsigned)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), (unsigned)BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B),
           (unsigned)gSaveContext.save.saveInfo.playerData.swordHealth,
           (unsigned)gSaveContext.save.saveInfo.stolenItems);
}

/** Arms (or disarms) the rando behaviours the way a save load does, in
 *  OnSaveLoadHandler's order (Rando/Rando.cpp): Rando::MiscBehavior::OnFileLoad,
 *  whose COND_HOOKs register the rando's Before/AfterEndOfCycleSave under
 *  IS_RANDO and unregister them otherwise, then ShipInit::Init("IS_RANDO"),
 *  which re-drives every `CVAR || IS_RANDO` registrar (EndOfCycle.cpp's Razor
 *  restore among them). */
void DriveRandoLoad(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    Rando::MiscBehavior::OnFileLoad();
    ShipInit::Init("IS_RANDO");
}

/** A live save with `equipValue` as its sword and `bItem` on the human B. */
void ArmSword(u8 equipValue, u8 bItem, u32 stolenItems) {
    ArmLiveSave();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, equipValue);
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = bItem;
    gSaveContext.save.saveInfo.playerData.swordHealth = (equipValue == EQUIP_VALUE_SWORD_RAZOR) ? 60 : 0;
    gSaveContext.save.saveInfo.stolenItems = stolenItems;
    gSaveContext.save.saveInfo.playerData.health = 0x30;
}

int RunRazorLegs() {
    // ---- (a) the Razor on B ----------------------------------------------------
    ArmSword(EQUIP_VALUE_SWORD_RAZOR, ITEM_SWORD_RAZOR, 0);
    DriveRandoLoad(true);
    PlaySongOfTime();
    PrintSword("(a) rando, Razor equipped, after the Song of Time");
    KIR_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_RAZOR,
               "(a) #1215: the Song of Time turned the seed's Razor Sword back into the Kokiri Sword");
    KIR_ASSERT(BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_SWORD_RAZOR,
               "(a) #1215: the Razor's equip value survived but B does not hold the Razor");

    // ---- (b) the Razor stolen by Takkuri ---------------------------------------
    ArmSword(EQUIP_VALUE_SWORD_NONE, ITEM_NONE, (u32)ITEM_SWORD_RAZOR << 0x18);
    DriveRandoLoad(true);
    PlaySongOfTime();
    PrintSword("(b) rando, Razor stolen, after the Song of Time");
    KIR_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_RAZOR &&
                   BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_SWORD_RAZOR,
               "(b) #1215: a Razor Takkuri stole came back as the Kokiri Sword");

    // ---- (c) no sword, nothing stolen ------------------------------------------
    ArmSword(EQUIP_VALUE_SWORD_NONE, ITEM_NONE, 0);
    DriveRandoLoad(true);
    PlaySongOfTime();
    PrintSword("(c) rando, no sword, after the Song of Time");
    KIR_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE &&
                   BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_NONE,
               "(c) a rando file without a sword was handed one by the reset (the rando's no-sword override)");

    // ---- (d) control: vanilla --------------------------------------------------
    ArmSword(EQUIP_VALUE_SWORD_RAZOR, ITEM_SWORD_RAZOR, 0);
    DriveRandoLoad(false);
    PlaySongOfTime();
    PrintSword("(d) vanilla, Razor equipped, after the Song of Time");
    KIR_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI &&
                   BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_SWORD_KOKIRI,
               "(d) a vanilla file kept the Razor through the reset: the restore leaked outside rando");
    return 0;
}

} // namespace

extern "C" int MM_CycleRazorSword_Run(void) {
    sRow = "mm-cycle-razor-sword";
    printf("[TEST] %s: a rando file keeps the seed's Razor Sword (equipped or stolen) through the three-day reset; a "
           "vanilla file drops it to the Kokiri Sword (#1215)\n",
           sRow);
    ComboContext_Init();
    RegisterCycleHooksOnce();

    const int rc = RunRazorLegs();

    // Disarm whatever the legs armed, on pass and on fail.
    DriveRandoLoad(false);
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
