/**
 * @file test_entrance_warp_target.c
 * @brief Headless lock for #1169 XW1: the one-shot cross-game warp target.
 *
 * WHAT IT IS. A warp (an OoT warp song played in Majora's Mask, OoT's Song of
 * Soaring to a Termina owl) crosses through the door's machinery: the
 * departing game's transition calls Combo_CheckEntranceSwitch, which flushes,
 * freezes and signals as for the door, and the launcher switches and commits.
 * The one new piece is ComboWarp_Arm (src/common/entrance.cpp): a record
 * Entrance_CheckCrossGame checks before the door links. Nothing in either game
 * arms it yet (XW6 and XW7 will), so this row is its only caller.
 *
 * THE LEGS.
 *   1. CONSUME. An MM warp song armed for Minuet's pad: the record holds the
 *      hot-swap return entrance; the transition that checks its marker gets its
 *      own entrance back with the switch to OoT pending, the pad as target, the
 *      warp kind, and the target spent.
 *   2. A MISMATCH DISARMS. Another entrance of the same game, and the marker's
 *      value in the other game, each leave nothing pending and the target
 *      spent, so the marker afterwards (a later void-out) does not cross either.
 *      A door taken after a mismatch crosses as the door, with the door kind.
 *   3. ARMING IS REFUSED while a crossing is pending (the pending door is left
 *      as it was) and while F10 is requested, and for every malformed target:
 *      no game, one game twice, a target entrance of 0 (the launcher ignores
 *      it), the door kind, a kind that does not match its direction, an
 *      unknown kind. A valid target arms afterwards (the control).
 *   4. A PENDING WARP STAYS THE WARP when a later call on the same transition
 *      names a door's source entrance (the marker here IS the Clock Tower
 *      door); without a warp that door crosses as the door (the control).
 *   5. ComboEntrance_Init, Entrance_ClearPendingSwitch and its C twin clear an
 *      armed target.
 *   6. THE KIND TRAVELS WITH THE STARTUP ENTRANCE, tagged for its game: the
 *      3-arg setter carries it, and the 2- and 1-arg setters (the F10 return,
 *      the refused re-entry, the legacy callers) and the clear reset it to the
 *      door.
 *   7. THE REAL Combo_CheckEntranceSwitch: OoT's Song of Soaring to the Clock
 *      Town owl, into a playable MM, hands back the marker with the switch
 *      pending, the kind carried, the ready signal set and OoT's half frozen
 *      with the hot-swap return entrance.
 *   8. THE #954 REFUSAL stays the backstop: the same warp into an unplayable MM
 *      hands back OoT's hot-swap return entrance with nothing pending, nothing
 *      requested, nothing frozen and the target spent.
 *   9. THE COMMIT RUNS. The same warp, then the launcher's handling of an
 *      entrance switch (rsbs/src/main.cpp) and the REAL GameRunner_SwitchTo
 *      with mock games: the whole-file commit (Switch_CommitCrossing) writes
 *      one generation with sourceGame MM, and MM's startup entrance is the owl
 *      with the Soaring kind while OoT's reads the door.
 *
 * Leg 9 pins the departing half live. Liveness is the departing game's
 * gameMode, which the flush notes (Combo_FlushLiveStateForFreeze) and only a
 * game TU can set; a src/common row cannot, and an AllTests process does not
 * pin it. It is locked where it is set (oot-scene-flag-freeze, crossing-commit).
 * The leg re-freezes the warp's own bytes with the warp's return entrance, so
 * the commit reads the freeze the warp produced and nothing else.
 *
 * RED ON MAIN: entrance.h declares none of ComboWarp_*, the kind or the 3-arg
 * setter, so this file does not compile there; with the target checked AFTER
 * the door links leg 4 fails; without the disarm on a mismatch leg 2 fails;
 * without the pending check in ComboWarp_Arm leg 3 fails.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like test_crossing_refusal.c: ComboEntrance_Init, Entrance_CheckCrossGame and
 * the 3-arg Entrance_SetStartupEntrance are entrance.h's C++-linkage API, and
 * leg 9 uses rsbs::SaveManager.
 */

#include "../context.h"
#include "../entrance.h"
#include "../game_lifecycle.h"
#include "../save.h"
#include "../test_runner.h"

#include <cstdint>
#include <cstdio>
#include <vector>

extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex);
extern "C" uint16_t Switch_GetHotSwapReturnEntrance(GameId departing);

#define EWT_ASSERT(cond, msg)                                                       \
    do {                                                                            \
        if (!(cond)) {                                                              \
            printf("[TEST] FAIL (entrance-warp-target): %s  [%s]\n", (msg), #cond); \
            return TEST_FAIL;                                                       \
        }                                                                           \
    } while (0)

namespace {

// An MM entrance no door link claims: South Clock Town spawn 1,
// ENTRANCE(SOUTH_CLOCK_TOWN, 1), where an MM warp song's prompt would arm.
constexpr uint16_t kEwtMarkerMM = 0xD810;
// An OoT entrance no door link claims: Lake Hylia's warp pad (0x0604), where
// OoT's Song of Soaring would arm.
constexpr uint16_t kEwtMarkerOoT = 0x0604;
// Minuet of Forest's vanilla pad, ENTR_SACRED_FOREST_MEADOW_WARP_PAD.
constexpr uint16_t kEwtMinuetPad = 0x0600;
// The Clock Town owl, ENTRANCE(SOUTH_CLOCK_TOWN, 9) (z_en_test7.c's owl table).
constexpr uint16_t kEwtClockTownOwl = 0xD890;

inline const char* EwtScratchDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_entrance_warp_target");
}

// Fresh production links; nothing pending, requested, armed or frozen.
bool EwtReset() {
    ComboEntrance_Init();
    Context_ClearAllFrozenStates();
    return Entrance_GetLinkCount() == 0 && !Combo_IsCrossGameSwitch() && !Combo_IsGameSwitchRequested() &&
           !ComboWarp_IsArmed() && Entrance_RegisterDefaultLinks();
}

bool EwtNothingPending() {
    return !Combo_IsCrossGameSwitch() && !Combo_IsGameSwitchRequested() &&
           Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_DOOR;
}

bool EwtArmMinuet(uint16_t marker) {
    return ComboWarp_Arm(GAME_MM, marker, GAME_OOT, kEwtMinuetPad, COMBO_SWITCH_KIND_WARP_SONG);
}

bool EwtArmSoaring() {
    return ComboWarp_Arm(GAME_OOT, kEwtMarkerOoT, GAME_MM, kEwtClockTownOwl, COMBO_SWITCH_KIND_SOARING);
}

// ---- Mock games for leg 9: the real GameRunner_SwitchTo around them ---------
int EwtInit(int, char**) {
    return 0;
}
void EwtNoop(void) {
}

GameOps sEwtOoT = { "oot", "Mock OoT", EwtInit, EwtNoop, EwtNoop, EwtNoop, EwtNoop };
GameOps sEwtMM = { "mm", "Mock MM", EwtInit, EwtNoop, EwtNoop, EwtNoop, EwtNoop };

struct EwtBadArm {
    GameId source;
    GameId target;
    uint16_t targetEntrance;
    uint8_t kind;
    const char* what;
};

TestResult EwtRun(void) {
    // ---- 1. Consume ----------------------------------------------------------
    EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
    EWT_ASSERT(EwtArmMinuet(kEwtMarkerMM), "an MM warp song to Minuet's pad must arm");
    ComboWarpTarget armed = {};
    EWT_ASSERT(ComboWarp_Peek(&armed) && armed.armed, "the armed target must read back armed");
    printf("[TEST] entrance-warp-target: armed %d->%d marker 0x%04X target 0x%04X return 0x%04X kind %u\n",
           (int)armed.sourceGame, (int)armed.targetGame, armed.marker, armed.targetEntrance, armed.returnEntrance,
           (unsigned)armed.kind);
    EWT_ASSERT(armed.sourceGame == GAME_MM && armed.marker == kEwtMarkerMM && armed.targetGame == GAME_OOT &&
                   armed.targetEntrance == kEwtMinuetPad && armed.kind == COMBO_SWITCH_KIND_WARP_SONG,
               "the record must hold what was armed");
    EWT_ASSERT(armed.returnEntrance == Switch_GetHotSwapReturnEntrance(GAME_MM) &&
                   armed.returnEntrance == MM_ENTR_SOUTH_CLOCK_TOWN_0,
               "the record's return entrance must be MM's hot-swap return entrance (0xD800)");
    EWT_ASSERT(Entrance_CheckCrossGame(GAME_MM, kEwtMarkerMM) == kEwtMarkerMM,
               "the transition that consumes the warp must load its own entrance");
    EWT_ASSERT(Combo_IsCrossGameSwitch() && Entrance_GetSwitchTargetGame() == GAME_OOT &&
                   Entrance_GetSwitchTargetEntrance() == kEwtMinuetPad &&
                   Entrance_GetSwitchReturnEntrance() == MM_ENTR_SOUTH_CLOCK_TOWN_0,
               "a (game, marker) match must leave the switch to Minuet's pad pending with the hot-swap return");
    EWT_ASSERT(Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_WARP_SONG &&
                   gPendingSwitch.kind == COMBO_SWITCH_KIND_WARP_SONG,
               "the pending switch must carry the warp kind");
    EWT_ASSERT(!gPendingSwitch.readyToSwitch, "the match only requests the switch; the freeze driver signals ready");
    EWT_ASSERT(!ComboWarp_IsArmed(), "the target is one-shot: consuming it disarms it");

    // ---- 2. A mismatch disarms -------------------------------------------------
    EWT_ASSERT(EwtReset() && EwtArmMinuet(kEwtMarkerMM), "setup: arm the warp song again");
    EWT_ASSERT(Entrance_CheckCrossGame(GAME_MM, kEwtMarkerMM + 0x10) == kEwtMarkerMM + 0x10,
               "another entrance must come back unchanged");
    EWT_ASSERT(EwtNothingPending(), "another entrance of the same game must not cross");
    EWT_ASSERT(!ComboWarp_IsArmed(), "another entrance of the same game must disarm the target");
    (void)Entrance_CheckCrossGame(GAME_MM, kEwtMarkerMM);
    EWT_ASSERT(EwtNothingPending(), "a later void-out to the marker must not cross on a target spent by a mismatch");

    EWT_ASSERT(EwtReset() && EwtArmMinuet(kEwtMarkerMM), "setup: arm the warp song again");
    (void)Entrance_CheckCrossGame(GAME_OOT, kEwtMarkerMM);
    EWT_ASSERT(EwtNothingPending() && !ComboWarp_IsArmed(),
               "the marker's value in the OTHER game must not cross and must disarm the target");

    EWT_ASSERT(EwtReset() && EwtArmMinuet(kEwtMarkerMM), "setup: arm the warp song again");
    (void)Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    EWT_ASSERT(Combo_IsCrossGameSwitch() && Entrance_GetSwitchTargetGame() == GAME_OOT &&
                   Entrance_GetSwitchTargetEntrance() == OOT_ENTR_MARKET_FROM_MASK_SHOP &&
                   Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_DOOR && !ComboWarp_IsArmed(),
               "the Clock Tower door taken while a warp is armed elsewhere must cross as the door, door kind");

    // ---- 3. Arming refused -----------------------------------------------------
    EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
    (void)Entrance_CheckCrossGame(GAME_OOT, OOT_ENTR_HAPPY_MASK_SHOP);
    EWT_ASSERT(Combo_IsCrossGameSwitch(), "setup: the Happy Mask Shop door leaves a crossing pending");
    EWT_ASSERT(!EwtArmMinuet(kEwtMarkerMM) && !ComboWarp_IsArmed(),
               "arming must be refused while a crossing is pending");
    EWT_ASSERT(Entrance_GetSwitchTargetGame() == GAME_MM &&
                   Entrance_GetSwitchTargetEntrance() == MM_ENTR_SOUTH_CLOCK_TOWN_0 &&
                   Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_DOOR,
               "a refused arm must leave the pending door crossing as it was");

    EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
    Combo_RequestGameSwitch();
    EWT_ASSERT(!EwtArmSoaring() && !ComboWarp_IsArmed(), "arming must be refused while F10 is requested");
    Combo_ClearGameSwitchRequest();

    const EwtBadArm kBad[] = {
        { GAME_NONE, GAME_OOT, kEwtMinuetPad, COMBO_SWITCH_KIND_WARP_SONG, "no source game" },
        { GAME_MM, GAME_NONE, kEwtMinuetPad, COMBO_SWITCH_KIND_WARP_SONG, "no target game" },
        { GAME_MM, GAME_MM, kEwtMinuetPad, COMBO_SWITCH_KIND_WARP_SONG, "one game twice" },
        { GAME_MM, GAME_OOT, 0, COMBO_SWITCH_KIND_WARP_SONG, "a target entrance of 0, which the launcher ignores" },
        { GAME_MM, GAME_OOT, kEwtMinuetPad, COMBO_SWITCH_KIND_DOOR, "the door kind" },
        { GAME_MM, GAME_OOT, kEwtMinuetPad, COMBO_SWITCH_KIND_SOARING, "Soaring from MM" },
        { GAME_OOT, GAME_MM, kEwtClockTownOwl, COMBO_SWITCH_KIND_WARP_SONG, "a warp song from OoT" },
        { GAME_MM, GAME_OOT, kEwtMinuetPad, 7, "an unknown kind" },
    };
    for (const EwtBadArm& bad : kBad) {
        EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
        const bool took = ComboWarp_Arm(bad.source, kEwtMarkerMM, bad.target, bad.targetEntrance, bad.kind);
        printf("[TEST] entrance-warp-target: arm with %s: %s\n", bad.what, took ? "ARMED" : "not armed");
        EWT_ASSERT(!took && !ComboWarp_IsArmed(), "a malformed warp target must not arm");
    }
    EWT_ASSERT(EwtReset() && EwtArmSoaring() && ComboWarp_IsArmed(), "control: a valid Soaring target arms");

    // ---- 4. A pending warp stays the warp -------------------------------------
    EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
    (void)Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    EWT_ASSERT(Combo_IsCrossGameSwitch() && Entrance_GetSwitchTargetEntrance() == OOT_ENTR_MARKET_FROM_MASK_SHOP,
               "control: without a warp the Clock Tower door crosses to the Market");
    EWT_ASSERT(EwtReset() && EwtArmMinuet(MM_ENTR_CLOCK_TOWER_INTERIOR_1), "setup: a warp whose marker is the door");
    (void)Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    EWT_ASSERT(Entrance_GetSwitchTargetEntrance() == kEwtMinuetPad &&
                   Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_WARP_SONG,
               "the target is checked before the door links: the warp must win its own marker");
    (void)Entrance_CheckCrossGame(GAME_MM, MM_ENTR_CLOCK_TOWER_INTERIOR_1);
    EWT_ASSERT(Combo_IsCrossGameSwitch() && Entrance_GetSwitchTargetEntrance() == kEwtMinuetPad &&
                   Entrance_GetSwitchReturnEntrance() == MM_ENTR_SOUTH_CLOCK_TOWN_0 &&
                   Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_WARP_SONG,
               "a later call on the same transition must not turn the pending warp into the door's crossing");

    // ---- 5. The clears ----------------------------------------------------------
    EWT_ASSERT(EwtReset() && EwtArmSoaring(), "setup: arm Soaring");
    Entrance_ClearPendingSwitch();
    EWT_ASSERT(!ComboWarp_IsArmed(), "Entrance_ClearPendingSwitch must clear the target");
    EWT_ASSERT(EwtArmSoaring(), "setup: arm Soaring again");
    Combo_ClearPendingSwitch();
    EWT_ASSERT(!ComboWarp_IsArmed(), "Combo_ClearPendingSwitch (the #954 refusal's clear) must clear the target");
    EWT_ASSERT(EwtArmSoaring(), "setup: arm Soaring again");
    ComboEntrance_Init();
    EWT_ASSERT(!ComboWarp_IsArmed(), "ComboEntrance_Init must clear the target");

    // ---- 6. The kind travels with the startup entrance ------------------------
    EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
    Entrance_SetStartupEntrance(kEwtMinuetPad, GAME_OOT, COMBO_SWITCH_KIND_WARP_SONG);
    EWT_ASSERT(Combo_GetStartupEntranceKindForGame("oot") == COMBO_SWITCH_KIND_WARP_SONG &&
                   Entrance_GetStartupEntranceKindForGame(GAME_OOT) == COMBO_SWITCH_KIND_WARP_SONG,
               "the 3-arg setter must carry the warp kind to the game it tags");
    EWT_ASSERT(Combo_GetStartupEntranceKindForGame("mm") == COMBO_SWITCH_KIND_DOOR,
               "a startup entrance tagged for OoT must read as the door for MM");
    EWT_ASSERT(Combo_GetStartupEntranceKindForGame(nullptr) == COMBO_SWITCH_KIND_DOOR &&
                   Combo_GetStartupEntranceKindForGame("zelda") == COMBO_SWITCH_KIND_DOOR,
               "an unknown game id must read as the door");
    Combo_ClearStartupEntrance();
    EWT_ASSERT(!Combo_HasStartupEntranceForGame("oot") &&
                   Combo_GetStartupEntranceKindForGame("oot") == COMBO_SWITCH_KIND_DOOR,
               "clearing the startup entrance must clear its kind");
    Entrance_SetStartupEntrance(kEwtMinuetPad, GAME_OOT, COMBO_SWITCH_KIND_WARP_SONG);
    Entrance_SetStartupEntrance(OOT_ENTR_MARKET_FROM_MASK_SHOP, GAME_OOT);
    EWT_ASSERT(Combo_GetStartupEntranceKindForGame("oot") == COMBO_SWITCH_KIND_DOOR,
               "the 2-arg setter (the F10 return, the refused re-entry) must set the door kind");
    Entrance_SetStartupEntrance(kEwtMinuetPad, GAME_OOT, COMBO_SWITCH_KIND_WARP_SONG);
    Combo_SetStartupEntrance(OOT_ENTR_MARKET_FROM_MASK_SHOP);
    EWT_ASSERT(Combo_GetStartupEntranceKindForGame("oot") == COMBO_SWITCH_KIND_DOOR,
               "the legacy 1-arg setter must set the door kind");
    Combo_ClearStartupEntrance();

    // ---- 7. The real Combo_CheckEntranceSwitch: Soaring into a playable MM ----
    Context_SetCurrentGame(GAME_OOT);
    Switch_SetTargetStatusForTest(SWITCH_TARGET_PLAYABLE);
    EWT_ASSERT(EwtReset() && EwtArmSoaring(), "setup: arm Soaring to the Clock Town owl");
    int prevSlot = RsbsSave_GetActiveSlot(); // the crossing publishes OoT's fileNum
    const uint16_t crossed = Combo_CheckEntranceSwitch(kEwtMarkerOoT);
    RsbsSave_SetActiveSlot(prevSlot);
    printf("[TEST] entrance-warp-target: Soaring from 0x%04X loads 0x%04X; pending %d -> %d 0x%04X kind %u; OoT "
           "frozen %d return 0x%04X\n",
           kEwtMarkerOoT, crossed, (int)Combo_IsCrossGameSwitch(), (int)Entrance_GetSwitchTargetGame(),
           Entrance_GetSwitchTargetEntrance(), (unsigned)Entrance_GetSwitchKind(), Context_HasFrozenState(GAME_OOT),
           Context_GetFrozenReturnEntrance(GAME_OOT));
    EWT_ASSERT(crossed == kEwtMarkerOoT, "a warp that goes ahead must hand its own entrance back");
    EWT_ASSERT(Combo_IsCrossGameSwitch() && Entrance_GetSwitchTargetGame() == GAME_MM &&
                   Entrance_GetSwitchTargetEntrance() == kEwtClockTownOwl &&
                   Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_SOARING,
               "Soaring must leave the switch to the Clock Town owl pending with the Soaring kind");
    EWT_ASSERT(gPendingSwitch.readyToSwitch, "the freeze driver must signal ready, as for the door");
    EWT_ASSERT(Context_HasFrozenState(GAME_OOT) == 1 &&
                   Context_GetFrozenReturnEntrance(GAME_OOT) == Switch_GetHotSwapReturnEntrance(GAME_OOT),
               "the departure must freeze OoT's half with OoT's hot-swap return entrance");

    // ---- 8. The #954 refusal --------------------------------------------------
    const SwitchTargetStatus kUnplayable[] = { SWITCH_TARGET_NO_GAME_ARCHIVE, SWITCH_TARGET_NO_PORT_ARCHIVE,
                                               SWITCH_TARGET_OOT_NOT_BOOTED };
    for (SwitchTargetStatus status : kUnplayable) {
        EWT_ASSERT(EwtReset() && EwtArmSoaring(), "setup: arm Soaring to the Clock Town owl");
        Switch_SetTargetStatusForTest(status);
        prevSlot = RsbsSave_GetActiveSlot();
        const uint16_t loaded = Combo_CheckEntranceSwitch(kEwtMarkerOoT);
        RsbsSave_SetActiveSlot(prevSlot);
        printf("[TEST] entrance-warp-target: Soaring into MM with target status %d loads 0x%04X\n", (int)status,
               loaded);
        EWT_ASSERT(loaded == Switch_GetHotSwapReturnEntrance(GAME_OOT),
                   "#954: a refused warp must hand back the departing game's hot-swap return entrance");
        EWT_ASSERT(EwtNothingPending(), "#954: a refused warp must leave no switch pending or requested");
        EWT_ASSERT(Context_HasFrozenState(GAME_OOT) == 0, "#954: a refused warp must freeze nothing");
        EWT_ASSERT(!ComboWarp_IsArmed(), "#954: a refused warp must leave the target spent");
    }
    Switch_SetTargetStatusForTest(SWITCH_TARGET_PLAYABLE);

    // ---- 9. The commit runs ---------------------------------------------------
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(EwtScratchDir());
    Context_InitFrozenStates();
    EWT_ASSERT(EwtReset(), "setup: the production links register on a clean table");
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.ArmSlotOnCreate(0);
    mgr.SetActiveSlot(0);
    {
        std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, 0x1B);
        std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, 0x2B);
        Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
        Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
    }
    gComboCtx.sourceGame = GAME_OOT;
    EWT_ASSERT(mgr.Save(0), "setup: a baseline commit of the slot");
    const uint32_t generation = gComboCtx.commitGeneration;

    EWT_ASSERT(EwtArmSoaring(), "setup: arm Soaring to the Clock Town owl");
    (void)Combo_CheckEntranceSwitch(kEwtMarkerOoT);
    mgr.SetActiveSlot(0); // the crossing published OoT's fileNum; the slot under test is 0
    EWT_ASSERT(Combo_IsCrossGameSwitch() && Context_HasFrozenState(GAME_OOT) == 1,
               "setup: the warp's departure leaves the switch pending and OoT's half frozen");
    {
        // The departing half pinned live (see the file comment): the warp's own
        // bytes, re-frozen with the warp's return entrance.
        std::vector<uint8_t> half(OOT_SAVE_CONTEXT_SIZE, 0);
        const uint16_t returnEntrance = Context_GetFrozenReturnEntrance(GAME_OOT);
        EWT_ASSERT(Context_RestoreState(GAME_OOT, half.data(), half.size()) == 1,
                   "setup: the warp's freeze reads back");
        Context_FreezeState(GAME_OOT, returnEntrance, half.data(), half.size());
    }

    // The launcher's handling of an entrance switch (rsbs/src/main.cpp).
    const GameId next = Entrance_GetSwitchTargetGame();
    const uint16_t targetEntrance = Entrance_GetSwitchTargetEntrance();
    const uint8_t switchKind = Entrance_GetSwitchKind();
    Combo_ClearGameSwitchRequest();
    Entrance_ClearPendingSwitch();
    Entrance_SetStartupEntrance(targetEntrance, next, switchKind);

    GameRunner runner;
    GameRunner_Init(&runner);
    GameRunner_RegisterGame(&runner, GAME_OOT, &sEwtOoT);
    GameRunner_RegisterGame(&runner, GAME_MM, &sEwtMM);
    EWT_ASSERT(GameRunner_StartGame(&runner, GAME_OOT, 0, nullptr) == 0, "setup: the mock OoT starts");
    EWT_ASSERT(GameRunner_SwitchTo(&runner, next, 0, nullptr) == 0, "the switch to MM must succeed");
    printf("[TEST] entrance-warp-target: commit generation %u -> %u, sourceGame %d; MM startup 0x%04X kind %d\n",
           (unsigned)generation, (unsigned)gComboCtx.commitGeneration, (int)gComboCtx.sourceGame,
           Combo_GetStartupEntranceForGame("mm"), Combo_GetStartupEntranceKindForGame("mm"));
    EWT_ASSERT(gComboCtx.commitGeneration == generation + 1 && gComboCtx.sourceGame == GAME_MM,
               "the warp's crossing must be a whole-file commit, as the door's is (#837)");
    EWT_ASSERT(Combo_HasStartupEntranceForGame("mm") && Combo_GetStartupEntranceForGame("mm") == kEwtClockTownOwl &&
                   Combo_GetStartupEntranceKindForGame("mm") == COMBO_SWITCH_KIND_SOARING,
               "MM's startup entrance must be the owl, carrying the Soaring kind");
    EWT_ASSERT(Combo_GetStartupEntranceKindForGame("oot") == COMBO_SWITCH_KIND_DOOR,
               "a startup entrance tagged for MM must read as the door for OoT");
    return TEST_PASS;
}

} // namespace

TestResult Test_EntranceWarpTarget(void) {
    printf("[TEST] entrance-warp-target: the one-shot cross-game warp target (#1169 XW1)\n");
    const GameId prevGame = Context_GetCurrentGame();
    const int prevSlot = RsbsSave_GetActiveSlot();
    const TestResult result = EwtRun();
    // Leave the process as the next row expects it, on pass or fail. The slot is
    // deleted from this row's scratch directory only, whichever leg failed.
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(EwtScratchDir());
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetActiveSlot(prevSlot);
    mgr.SetSaveDirectory("Save");
    ComboContext_Init();
    Switch_SetTargetStatusForTest(-1);
    Context_SetCurrentGame(prevGame);
    ComboEntrance_Init();
    (void)Entrance_RegisterDefaultLinks();
    Context_ClearAllFrozenStates();
    if (result == TEST_PASS) {
        printf("[TEST] entrance-warp-target: PASS\n");
    }
    return result;
}
