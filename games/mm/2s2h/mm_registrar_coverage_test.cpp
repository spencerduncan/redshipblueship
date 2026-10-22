/**
 * @file mm_registrar_coverage_test.cpp
 * ROM-free, display-free lock for #516: MM's single-exe bring-up must actually
 * POPULATE the hook registries that BenPort.cpp's exclusion emptied.
 * CTest label "redship", row MMRegistrarCoverage in CMake/SingleExecutable.cmake,
 * dispatch "mm-registrar-coverage" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#516). games/mm/2s2h/BenPort.cpp is excluded from the single
 * exe (games/mm/CMakeLists.txt), and it was the SOLE caller of InitOTR's
 * registration sequence. 2ship_enh is a plain STATIC archive, so with no
 * inbound reference those TUs were link-elided outright: CustomItem::
 * RegisterHooks, S2H::CustomMessage::RegisterHooks, InitEnhancements (and
 * through it RegisterSavingEnhancements / RegisterAutosave) and
 * GfxPatcher_ApplyNecessaryAuthenticPatches were all 0-hit in redship.map.
 * Player-visible: every randomized custom item uncollectable, every rando/hint
 * textbox rendering vanilla message 0x004B, no owl-save persistence, no
 * autosave. The registrars have since been re-homed into MM_Rando_Init
 * (games/mm/2s2h/GameExports_SingleExe.cpp).
 *
 * WHY THIS ROW EXISTS ALONGSIDE THE CI SYMBOL GATE. .github/scripts/
 * check-registrar-elision.sh greps the linked binary for a per-symbol
 * allowlist. That proves the registrars LINKED. It cannot prove they RAN --
 * a call moved under a condition that is never true, or a once-guard that
 * latches before the call, both keep the symbol in the binary and leave every
 * registry as empty as full elision did. This row closes that gap the way the
 * script's own header recommends for ForeignItemsSingleExe.cpp: "ask the
 * running binary whether MM's pool actually registered. A dropped initializer
 * is a red test, not a subtle count."
 *
 * It is also the ONLY possible gate for RegisterAutosave. That symbol cannot
 * be allowlisted in the CI script: OoT ships a static RegisterAutosave
 * (games/oot/soh/Enhancements/QoL/Autosave.cpp:80), so a demangled-name grep
 * is satisfied by OoT's copy whether or not MM's ever links. Attribution by
 * name is impossible; attribution by REGISTRY CONTENT is exact, because
 * OnGameStateDrawFinish has exactly one registrant in the whole MM tree
 * (SavingEnhancements.cpp, inside RegisterAutosave's CVar-gated branch).
 * That closes #516's open question 1 without renaming upstream-tracked code.
 *
 * ATTRIBUTION -- why each probe names exactly one registrar.
 *   - BeforeMoonCrashSaveReset: sole registrant in all of games/mm is
 *     SavingEnhancements.cpp (RegisterSavingEnhancements). Non-empty => it ran.
 *   - OnGameStateDrawFinish:    sole registrant is SavingEnhancements.cpp
 *     (RegisterAutosave, CVar-gated). Non-empty => it ran.
 *   - OnOpenText[0x004B]:       sole registrant for that id is
 *     CustomMessage.cpp (CustomMessage::RegisterHooks).
 *   - ShouldActorInit[EN_ITEM00]: FOUR registrants exist -- CustomItem.cpp
 *     (unconditional) plus ChuDrops.cpp, JPGrottos.cpp (each CVar-gated) and
 *     Rando/ActorBehavior/EnItem00.cpp (IS_RANDO-gated). The two CVar-gated
 *     ones are forced OFF below, so a non-empty bucket can only be
 *     CustomItem's. Without that forcing this probe could pass on a default
 *     flip while CustomItem::RegisterHooks stayed dead -- the exact vacuity
 *     #516 is about. The IS_RANDO one cannot be forced from here without
 *     touching gSaveContext (MM's z64save.h carries no extern "C" guard, so a
 *     C++ TU that includes global.h at file scope cannot also take a C-linkage
 *     declaration of it), so it is caught rather than prevented: the bucket is
 *     asserted to hold EXACTLY ONE registrant, and a second one fails loudly as
 *     a precondition violation instead of silently propping up the probe.
 *
 * NON-VACUITY. Every probe asserts its registry is EMPTY before MM_Rando_Init
 * and non-empty after, so a registry populated by something else, or by an
 * earlier phase of the harness, cannot satisfy it. Verified red before green:
 * commenting out any one of the four calls in MM_Rando_Init fails this row at
 * that probe's own FAIL code, and re-adding it turns it green. The two Phase-3
 * probes below follow the same contract on their own state -- not run before,
 * run after (FAIL 8), patched-after-scan after (FAIL 9).
 *
 * DELIBERATELY NOT REFERENCED HERE. This row drives only the production entry
 * point MM_Rando_Init(); it never calls CustomItem::RegisterHooks and friends
 * directly. That is load-bearing, not stylistic: a direct call from this
 * always-linked test TU would itself be an inbound reference that keeps those
 * symbols in the binary, which would make the CI symbol gate pass on a build
 * where production had dropped every call. The test must not prop up what the
 * gate measures.
 *
 * PHASE 3 (#618) -- THE TWO ENTRIES WITH NO REGISTRY. OTRExtScanner and
 * PlayerCustomFlipbooks_Patch were the complete remainder of BenPort's InitOTR
 * list after PR #616's audit: both depend on the SHARED ExtensionCache having
 * been re-scanned once MM's archives are mounted, which is why they could not
 * ride Phases 1-2. Neither is a hook registrar, so neither has a registry to
 * probe. What this row probes instead is that MM_Rando_Init REACHED them, and
 * in the right order, through the state MM_ExtensionRescan_AfterArchiveMount
 * records (GameExports_SingleExe.cpp). The order matters on its own:
 * PlayerCustomFlipbooks_PatchOnce latches on its first call, so running it
 * before the rescan would permanently pin MM's faces to vanilla against a cache
 * that had never seen mm.o2r. That the rescan actually ADDS MM-scoped entries
 * and removes none of OoT's is the separate MMExtensionRescan row's job
 * (src/common/tests/test_mm_extension_rescan.c), which needs a staged archive
 * and so SKIPs where this row cannot.
 *
 * PHASE 4 (#1005-#1009) -- THE FIVE CHEATS TUs. Cheats/Infinite.cpp,
 * ClimbAnywhere.cpp, HookshotAnywhere.cpp, MoonJump.cpp and
 * EasyFrameAdvance.cpp register only through a file-scope RegisterShipInitFunc
 * keyed on one gCheats.* CVar each, in the plain `2ship_enh` archive, so the
 * linker dropped all five and the shared Cheats rows did nothing in Majora's
 * Mask. Every one of these registrars is CVar-gated (the COND_* macros register
 * only when the key reads true), so the registries are empty after MM_Rando_Init
 * for ANY link. What distinguishes a linked TU is that it answers
 * ShipInit::Init(<its key>): the probe turns the CVar on, drives exactly that
 * key (what OoT's ShipInit::Init forwards when the player toggles the row) and
 * requires the matching registry to rise by exactly one. Each key is read by
 * one TU tree-wide (BenMenu.cpp only draws it), so the delta is attributable;
 * nothing the five TUs define is named here (a reference would un-elide them).
 *   - gCheats.InfiniteHealth / InfiniteMagic / InfiniteMoney: three
 *     OnGameStateUpdate registrants, one per key (Infinite.cpp).
 *   - gCheats.ClimbEverything: ShouldVanillaBehavior[VB_BE_CLIMBABLE_SURFACE].
 *   - gCheats.HookshotEverything: ShouldVanillaBehavior[VB_BE_HOOKSHOT_SURFACE].
 *   - gCheats.MoonJumpOnL: OnActorUpdate[ACTOR_PLAYER].
 *   - gCheats.EasyFrameAdvance: OnGameStateMainStart.
 * The probe puts every key back to 0 and re-drives it, so the row is order-free.
 *
 * PHASE 5 (#1018-#1022) -- THE SKIPS & SPEED-UPS TUs. Cutscenes/HideTitleCards,
 * SkipIntroSequence, SkipEntranceCutscenes, SkipOnePointCutscenes and the
 * StoryCutscenes/* set register only through a file-scope RegisterShipInitFunc
 * keyed on the converged gEnhancements.TimeSavers.* key SoH's row writes, in the
 * plain `2ship_enh` archive, so the linker dropped them and the rows did
 * nothing in Majora's Mask. Each registrar is CVar-gated, so what proves a TU is
 * linked is that ShipInit::Init(<its key>) with the key ON raises the registry
 * the TU writes to by EXACTLY the number of registrations the table below names
 * for that key. Exact, not "at least": the Story key is also read by linked TUs
 * (SkipGiantsChamber) and by the #1071 song teachers, which are in the table, so
 * a TU of this set falling out of the link leaves the delta short by its own
 * count. Adding a Story registrant means adding its line to the table. Nothing a
 * TU defines is named here (a reference would un-elide it).
 *   - one key, one TU: Title Card (VB_SHOW_TITLE_CARD), Intro
 *     (VB_PLAY_TRANSITION_CS), Entrances (VB_PLAY_ENTRANCE_CS, VB_START_CUTSCENE),
 *     One Point (VB_CAMERA_SET_FOCAL_ACTOR, VB_START_CUTSCENE).
 *   - Story: the remaining StoryCutscenes TUs, minus HealingMikauAudio, which is
 *     unconditional and is probed separately: OnActorInit[ACTOR_EN_ZOG] has no
 *     other registrant, empty before MM_Rando_Init and one after.
 *
 * PHASE 6 (#1010-#1017) -- THE EIGHT ENHANCEMENT TUs. Timesavers/FastChests,
 * Graphics/EnemyHealthBars, Graphics/BowReticle, Graphics/DisableBlackBars,
 * Player/InstantPutaway, Equipment/ItemUnequip, Restorations/PauseBufferInputs
 * and DifficultyOptions/HyperEnemies register only through a file-scope
 * RegisterShipInitFunc keyed on the converged gEnhancements.* key SoH's row
 * writes (kConvergedKeys), in the plain `2ship_enh` archive, so the linker
 * dropped all eight and the rows did nothing in Majora's Mask. Same probe as
 * Phase 5: the key ON plus ShipInit::Init(key) must raise the registry the TU
 * writes to by EXACTLY the count the table names. Every key is read by one TU
 * tree-wide (BenMenu.cpp only draws it), so the delta is attributable. Nothing a
 * TU defines is named here (a reference would un-elide it).
 *   - FastChests: VB_PLAY_SLOW_CHEST_CS.   DisableBlackBars: VB_DISABLE_LETTERBOX.
 *   - InstantPutaway: VB_RESET_PUTAWAY_TIMER.
 *   - ItemUnequip: VB_KALEIDO_EQUIP_ITEM_TO_BUTTON; the same TU's second key,
 *     DpadEquips, arms VB_KALEIDO_SWITCH_PAGE_WITH_DPAD.
 *   - PauseBufferWindow: VB_KALEIDO_UNPAUSE_CLOSE and OnGameStateMainStart.
 *   - HyperEnemies: OnActorUpdate (unkeyed).
 *   - BowReticle: OnPlayerPostLimbDraw keyed on PLAYER_LIMB_RIGHT_HAND, the
 *     ForID leg MM_GameHooks_ExecuteOnPlayerPostLimbDraw runs from
 *     Player_PostLimbDrawGameplay (z_player_lib.c): the draw hook of the aiming
 *     reticle.
 *   - EnemyHealthBars: OnInterfaceDrawStart (+1), and the TU's unconditional
 *     OnActorInit registrant must NOT grow when the key is toggled (it used to
 *     register again on every toggle; the unkeyed OnActorInit bucket is read
 *     before and after the drive).
 * Each key is driven TWICE per probe, so a registrar that stacks one
 * registration per toggle shows up as a second delta.
 *
 * PHASE 7 (#1024-#1026) -- THE THREE AUDIO EDITOR TUs. Audio/Sfx/MuteLowHpAlarm,
 * Audio/Sfx/DisableEnemyProximityMusic and Audio/AudioHook (the sequence-name
 * notification) register only through a file-scope RegisterShipInitFunc keyed on
 * the gAudioEditor.* key SoH's Audio Editor row writes (kSharedIntentKeys), in
 * the plain `2ship_enh` archive, so the linker dropped all three and the rows did
 * nothing in Majora's Mask. Same probe as Phases 5 and 6: the key ON plus
 * ShipInit::Init(key), driven twice, must raise the registry the TU writes to by
 * EXACTLY the count the table names. Nothing a TU defines is named here (a
 * reference would un-elide it).
 *   - gAudioEditor.LowHpAlarm: VB_PLAY_LOW_HP_ALARM (z_lifemeter.c).
 *   - gAudioEditor.EnemyBGMDisable: VB_PLAY_ENEMY_PROXIMITY_MUSIC (z_player.c).
 *   - gAudioEditor.SeqNameNotification: OnSeqPlayerInit, unkeyed (audio/lib/load.c).
 *     The duration row, gAudioEditor.SeqNameNotificationDuration, is READ by that
 *     same TU's handler at emit time, so it has no registration of its own.
 *
 * PHASE 8 (#1023) -- FREE LOOK. Camera/FreeLook.cpp registers only through a
 * file-scope RegisterShipInitFunc keyed on gSettings.FreeLook.Enabled (the
 * converged key SoH's Settings > Controls > Free Look row writes), in the plain
 * `2ship_enh` archive, so the linker dropped it and Free Look, Camera Distance
 * and Camera Transition Speed did nothing in Majora's Mask. Same probe as
 * Phases 5-7: the key ON plus ShipInit::Init(key), driven twice, must raise each
 * registry the TU writes to by EXACTLY the count the table names. The key is read
 * by one TU tree-wide (BenMenu.cpp only draws it), so the delta is attributable.
 * Nothing the TU defines is named here (a reference would un-elide it).
 *   - ShouldVanillaBehavior[VB_USE_CUSTOM_CAMERA] (z_camera.c, Camera_Update): the
 *     free-look camera itself. DebugCam.cpp also registers there but keys on
 *     gEnhancements.Camera.DebugCam.Enable and is not linked.
 *   - OnCameraChangeModeFlags, unkeyed (z_camera.c, Camera_ChangeMode): the
 *     per-mode "may free-look" reset.
 *
 * WHAT THIS DOES NOT COVER. GfxPatcher_ApplyNecessaryAuthenticPatches is a
 * one-shot resource patcher, not a hook registrar, and MM_Rando_Init gates it
 * behind MM_Rando_AssetsReady() because its ResourceMgr_Load*ByName helpers
 * null-deref with no mm.o2r. It therefore does not run in this ROM-free row and
 * has no registry to probe; it stays covered by the CI symbol allowlist alone.
 * Nor does this row prove the registered handlers are ever DISPATCHED -- that
 * is MMHookDispatch's invariant (#511/#512), and the two rows are complementary:
 * dispatch without registration and registration without dispatch are both
 * silent, and each was a real shipped bug.
 */

#include "global.h"

#include <cstdio>
#include <vector>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The S2H::GameHooks registry and the BenPort exclusion that motivates this
 * lock both exist only in the single-exe build. Outside it MM runs its own
 * InitOTR and there is nothing to re-home, so the row reports pass rather than
 * failing to link. test_runner.cpp declares this unconditionally.
 */
extern "C" int MM_RegistrarCoverage_RunHeadless(void) {
    printf("[TEST] mm-registrar-coverage: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
// The production bring-up under test. Declared, never defined here.
void MM_Rando_Init(void);
// ADR 0010 increment 2 (P13): the core/asset split's observable and the
// asset phase's #618 entry, both driven directly by this row.
int MM_Rando_AssetPhaseLatched(void);

// #618 (#516 Phase 3) state, recorded by MM_ExtensionRescan_AfterArchiveMount
// in GameExports_SingleExe.cpp. Declaring the ACCESSORS rather than the two
// restored entries keeps this row from becoming an inbound reference to
// PlayerCustomFlipbooks_Patch — the same discipline the rest of this file
// follows (see DELIBERATELY NOT REFERENCED HERE in the header).
int MM_ExtensionRescan_Scanned(void);
int MM_ExtensionRescan_FlipbooksPatchedAfterScan(void);
int MM_ExtensionRescan_CallCount(void);
// The asset phase's #618 entry point, driven by name below (ADR 0010 increment
// 2 moved it out of the core phase — see the call site for why).
void MM_ExtensionRescan_AfterArchiveMount(void);
}

namespace {

#define RC_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// Settled counts. Unregistration in S2H::GameHooks is DEFERRED -- Unregister
// only queues an id, and the queue is drained at the next Execute of that hook
// type. RegisterAutosave in particular unregisters-then-registers on every
// call, so counting the raw maps without draining first could credit a hook
// that is already slated for removal. Flushing makes each count the state a
// real dispatch would see.
template <typename H> size_t SettledUnkeyedCount() {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    return S2H::GameHooks::Registry<H>::functions.size();
}

template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

// The two CVar-gated OTHER ShouldActorInit[ACTOR_EN_ITEM00] registrants, forced
// off so the probe can only be satisfied by CustomItem's unconditional one.
// (The third competitor, EnItem00.cpp's IS_RANDO leg, is caught by the
// exactly-one assertion rather than prevented -- see the file header.)
constexpr const char* kChuDropsCVar = "gEnhancements.Equipment.ChuDrops";
constexpr const char* kJPGrottosCVar = "gEnhancements.Restorations.JPGrottos";

// RegisterAutosave registers nothing unless this is set -- it is a runtime
// toggle, not an unconditional registrar. Forced ON so its absence is a real
// failure rather than a configuration artifact.
constexpr const char* kAutosaveCVar = "gEnhancements.Autosave";

// ---- Phase 4 (#1005-#1009): the five Cheats TUs ---------------------------
// One probe per gCheats.* key. `count` reads the settled registry the key's
// registrar writes to; see PHASE 4 in the file header for the attribution.
struct CheatProbe {
    const char* key;
    const char* what;
    size_t (*count)();
};

size_t UpdateCount() {
    return SettledUnkeyedCount<GameInteractor::OnGameStateUpdate>();
}
size_t ClimbCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_BE_CLIMBABLE_SURFACE);
}
size_t HookshotCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_BE_HOOKSHOT_SURFACE);
}
size_t PlayerUpdateCount() {
    return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_PLAYER);
}
size_t MainStartCount() {
    return SettledUnkeyedCount<GameInteractor::OnGameStateMainStart>();
}

const CheatProbe kCheatProbes[] = {
    { "gCheats.InfiniteHealth", "Infinite Health: OnGameStateUpdate (Cheats/Infinite.cpp)", UpdateCount },
    { "gCheats.InfiniteMagic", "Infinite Magic: OnGameStateUpdate (Cheats/Infinite.cpp)", UpdateCount },
    { "gCheats.InfiniteMoney", "Infinite Money: OnGameStateUpdate (Cheats/Infinite.cpp)", UpdateCount },
    { "gCheats.ClimbEverything", "Climb Everything: VB_BE_CLIMBABLE_SURFACE (Cheats/ClimbAnywhere.cpp)", ClimbCount },
    { "gCheats.HookshotEverything", "Hookshot Everything: VB_BE_HOOKSHOT_SURFACE (Cheats/HookshotAnywhere.cpp)",
      HookshotCount },
    { "gCheats.MoonJumpOnL", "Moon Jump on L: OnActorUpdate[ACTOR_PLAYER] (Cheats/MoonJump.cpp)", PlayerUpdateCount },
    { "gCheats.EasyFrameAdvance", "Easy Frame Advance: OnGameStateMainStart (Cheats/EasyFrameAdvance.cpp)",
      MainStartCount },
};

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetCheatKeys() {
    for (const auto& probe : kCheatProbes) {
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
    }
    for (const auto& probe : kCheatProbes) {
        (void)probe.count();
    }
}

// ---- Phase 5 (#1018-#1022): the Skips & Speed-ups TUs ---------------------
// One probe per SoH-written key. Each registry row names the TUs that put a
// hook there when the key turns on; `expected` is their summed count.
struct SkipRegistry {
    const char* what;
    size_t (*count)();
    size_t expected;
};

struct SkipKeyProbe {
    const char* key;
    const char* row;
    std::vector<SkipRegistry> registries;
};

#define SKIP_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }
#define SKIP_ACTOR_INIT_COUNT(id) []() { return SettledCountForId<GameInteractor::OnActorInit>(id); }

const std::vector<SkipKeyProbe>& SkipProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gEnhancements.TimeSavers.DisableTitleCard",
          "Disable Title Card",
          { { "VB_SHOW_TITLE_CARD (Cutscenes/HideTitleCards.cpp)", SKIP_VB_COUNT(VB_SHOW_TITLE_CARD), 1 } } },
        { "gEnhancements.TimeSavers.SkipCutscene.Intro",
          "Skip Intro",
          { { "VB_PLAY_TRANSITION_CS (Cutscenes/SkipIntroSequence.cpp)", SKIP_VB_COUNT(VB_PLAY_TRANSITION_CS), 1 } } },
        { "gEnhancements.TimeSavers.SkipCutscene.Entrances",
          "Skip Entrance Cutscenes",
          { { "VB_PLAY_ENTRANCE_CS (Cutscenes/SkipEntranceCutscenes.cpp)", SKIP_VB_COUNT(VB_PLAY_ENTRANCE_CS), 1 },
            { "VB_START_CUTSCENE (Cutscenes/SkipEntranceCutscenes.cpp)", SKIP_VB_COUNT(VB_START_CUTSCENE), 1 } } },
        { "gEnhancements.TimeSavers.SkipCutscene.OnePoint",
          "Skip One Point Cutscenes",
          { { "VB_CAMERA_SET_FOCAL_ACTOR (Cutscenes/SkipOnePointCutscenes.cpp)",
              SKIP_VB_COUNT(VB_CAMERA_SET_FOCAL_ACTOR), 1 },
            { "VB_START_CUTSCENE (Cutscenes/SkipOnePointCutscenes.cpp)", SKIP_VB_COUNT(VB_START_CUTSCENE), 1 } } },
        // The Story key. Per registry, the contributing TUs (StoryCutscenes/ unless noted):
        //   VB_START_CUTSCENE 14 = Biggoron, DefeatCaptain, HealingMikau, IkanaCurse x2 (Pamela, Sharp),
        //     PrincessDelivery, RaisingWoodfall, StoppingMoon, WakingAndRidingTurtle (9, #1022) +
        //     LearningElegy, LearningGoronLullaby, LearningNewWaveBossaNova, LearningSonataOfAwakening,
        //     LearningSongOfStorms (5, #1071).
        //   VB_QUEUE_CUTSCENE 4 = BombBagTheft, HealingDarmani, IkanaCurse (3, #1022) + LearningEponasSong (#1071).
        //   VB_PLAY_TRANSITION_CS 5 = ClockTowerOpen, FirstSongOfTimeFlashbacks, MilkRun, MoonCrash (4, #1022) +
        //     SkipGiantsChamber (already linked).
        //   VB_GIVE_ITEM_FROM_OFFER 1 = SkipGiantsChamber (already linked, CVAR && !IS_RANDO).
        //   The 1-of-1 buckets: VB_SAKON_TAKE_DAMAGE (BombBagTheft), VB_CLOCK_TOWER_OPENING_CONSIDER_THIS_FIRST_CYCLE
        //     (ClockTowerOpen), VB_PLAY_DEFEAT_CAPTAIN_SEQUENCE (DefeatCaptain), VB_PLAY_SONG_OF_TIME_CS
        //     (LearningSongOfTime), OnActorInit[DM_STK] and [DM_CHAR02] (ClockTowerSkullKidEncounter),
        //     OnActorInit[EN_PO_COMPOSER] (IkanaCurse).
        { "gEnhancements.TimeSavers.SkipCutscene.Story",
          "Skip Story Cutscenes",
          { { "VB_START_CUTSCENE", SKIP_VB_COUNT(VB_START_CUTSCENE), 14 },
            { "VB_QUEUE_CUTSCENE", SKIP_VB_COUNT(VB_QUEUE_CUTSCENE), 4 },
            { "VB_PLAY_TRANSITION_CS", SKIP_VB_COUNT(VB_PLAY_TRANSITION_CS), 5 },
            { "VB_GIVE_ITEM_FROM_OFFER", SKIP_VB_COUNT(VB_GIVE_ITEM_FROM_OFFER), 1 },
            { "VB_SAKON_TAKE_DAMAGE (SkipBombBagTheftCutscene.cpp)", SKIP_VB_COUNT(VB_SAKON_TAKE_DAMAGE), 1 },
            { "VB_CLOCK_TOWER_OPENING_CONSIDER_THIS_FIRST_CYCLE (SkipClockTowerOpen.cpp)",
              SKIP_VB_COUNT(VB_CLOCK_TOWER_OPENING_CONSIDER_THIS_FIRST_CYCLE), 1 },
            { "VB_PLAY_DEFEAT_CAPTAIN_SEQUENCE (SkipDefeatCaptainSequence.cpp)",
              SKIP_VB_COUNT(VB_PLAY_DEFEAT_CAPTAIN_SEQUENCE), 1 },
            { "VB_PLAY_SONG_OF_TIME_CS (SkipLearningSongOfTime.cpp)", SKIP_VB_COUNT(VB_PLAY_SONG_OF_TIME_CS), 1 },
            { "OnActorInit[DM_STK] (SkipClockTowerSkullKidEncounter.cpp)", SKIP_ACTOR_INIT_COUNT(ACTOR_DM_STK), 1 },
            { "OnActorInit[DM_CHAR02] (SkipClockTowerSkullKidEncounter.cpp)", SKIP_ACTOR_INIT_COUNT(ACTOR_DM_CHAR02),
              1 },
            { "OnActorInit[EN_PO_COMPOSER] (SkipIkanaCurseCutscenes.cpp)", SKIP_ACTOR_INIT_COUNT(ACTOR_EN_PO_COMPOSER),
              1 } } },
    };
    return probes;
}

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetSkipKeys() {
    for (const auto& probe : SkipProbes()) {
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
    }
    for (const auto& probe : SkipProbes()) {
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
}

// ---- Phase 6 (#1010-#1017): the eight enhancement TUs ----------------------
// Same shape as Phase 5 (SkipKeyProbe). `expected` may be 0: that row asserts a
// registry does NOT grow when the key is driven (the EnemyHealthBars OnActorInit
// once-only registration).
#define ENH_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }

const std::vector<SkipKeyProbe>& EnhProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gEnhancements.FastChests",
          "Fast Chests",
          { { "VB_PLAY_SLOW_CHEST_CS (Timesavers/FastChests.cpp)", ENH_VB_COUNT(VB_PLAY_SLOW_CHEST_CS), 1 } } },
        { "gEnhancements.EnemyHealthBar",
          "Enemy Health Bars",
          { { "OnInterfaceDrawStart (Graphics/EnemyHealthBars.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnInterfaceDrawStart>(); }, 1 },
            { "OnActorInit, unkeyed: must not grow per toggle (Graphics/EnemyHealthBars.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnActorInit>(); }, 0 } } },
        { "gEnhancements.BowReticle",
          "Aiming Reticle for the Bow/Slingshot",
          { { "OnPlayerPostLimbDraw[PLAYER_LIMB_RIGHT_HAND] (Graphics/BowReticle.cpp)",
              []() { return SettledCountForId<GameInteractor::OnPlayerPostLimbDraw>(PLAYER_LIMB_RIGHT_HAND); }, 1 } } },
        { "gEnhancements.DisableBlackBars",
          "Disable Black Bar Letterboxes",
          { { "VB_DISABLE_LETTERBOX (Graphics/DisableBlackBars.cpp)", ENH_VB_COUNT(VB_DISABLE_LETTERBOX), 1 } } },
        { "gEnhancements.InstantPutaway",
          "Instant Putaway",
          { { "VB_RESET_PUTAWAY_TIMER (Player/InstantPutaway.cpp)", ENH_VB_COUNT(VB_RESET_PUTAWAY_TIMER), 1 } } },
        { "gEnhancements.ItemUnequip",
          "Unequip C-Items on Re-press",
          { { "VB_KALEIDO_EQUIP_ITEM_TO_BUTTON (Equipment/ItemUnequip.cpp)",
              ENH_VB_COUNT(VB_KALEIDO_EQUIP_ITEM_TO_BUTTON), 1 } } },
        { "gEnhancements.DpadEquips",
          "D-pad Equips (the dpad page-switch half of Equipment/ItemUnequip.cpp)",
          { { "VB_KALEIDO_SWITCH_PAGE_WITH_DPAD (Equipment/ItemUnequip.cpp)",
              ENH_VB_COUNT(VB_KALEIDO_SWITCH_PAGE_WITH_DPAD), 1 } } },
        { "gEnhancements.PauseBufferWindow",
          "Pause Buffer Input Window",
          { { "VB_KALEIDO_UNPAUSE_CLOSE (Restorations/PauseBufferInputs.cpp)", ENH_VB_COUNT(VB_KALEIDO_UNPAUSE_CLOSE),
              1 },
            { "OnGameStateMainStart (Restorations/PauseBufferInputs.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnGameStateMainStart>(); }, 1 } } },
        { "gEnhancements.HyperEnemies",
          "Hyper Enemies",
          { { "OnActorUpdate, unkeyed (DifficultyOptions/HyperEnemies.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnActorUpdate>(); }, 1 } } },
    };
    return probes;
}

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetEnhKeys() {
    for (const auto& probe : EnhProbes()) {
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
    }
    for (const auto& probe : EnhProbes()) {
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
}

// ---- Phase 7 (#1024-#1026): the three Audio Editor TUs ---------------------
// Same shape as Phase 5 (SkipKeyProbe).
const std::vector<SkipKeyProbe>& AudioProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gAudioEditor.LowHpAlarm",
          "Mute Low HP Alarm",
          { { "VB_PLAY_LOW_HP_ALARM (Audio/Sfx/MuteLowHpAlarm.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_PLAY_LOW_HP_ALARM); }, 1 } } },
        { "gAudioEditor.EnemyBGMDisable",
          "Disable Enemy Proximity Music",
          { { "VB_PLAY_ENEMY_PROXIMITY_MUSIC (Audio/Sfx/DisableEnemyProximityMusic.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_PLAY_ENEMY_PROXIMITY_MUSIC); },
              1 } } },
        { "gAudioEditor.SeqNameNotification",
          "Display Sequence Name in Notifications",
          { { "OnSeqPlayerInit, unkeyed (Audio/AudioHook.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnSeqPlayerInit>(); }, 1 } } },
    };
    return probes;
}

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetAudioKeys() {
    for (const auto& probe : AudioProbes()) {
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
    }
    for (const auto& probe : AudioProbes()) {
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
}

// ---- Phase 8 (#1023): Free Look ---------------------------------------------
// Same shape as Phase 5 (SkipKeyProbe).
const std::vector<SkipKeyProbe>& FreeLookProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gSettings.FreeLook.Enabled",
          "Free Look",
          { { "VB_USE_CUSTOM_CAMERA (Camera/FreeLook.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_USE_CUSTOM_CAMERA); }, 1 },
            { "OnCameraChangeModeFlags, unkeyed (Camera/FreeLook.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnCameraChangeModeFlags>(); }, 1 } } },
    };
    return probes;
}

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetFreeLookKeys() {
    for (const auto& probe : FreeLookProbes()) {
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
    }
    for (const auto& probe : FreeLookProbes()) {
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
}

} // namespace

extern "C" int MM_RegistrarCoverage_RunHeadless(void) {
    printf("[TEST] mm-registrar-coverage: MM bring-up populates the registries BenPort's exclusion emptied (#516)\n");

    auto ctx = Ship::Context::GetInstance();
    RC_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing — run the shared bring-up first");
    RC_ASSERT(ctx->GetConsoleVariables() != nullptr, 1,
              "ConsoleVariables missing — ShipInit registrars and the CVar forcing below need them");

    // ---- Harness configuration, before any registration runs ---------------
    // Competing EN_ITEM00 registrants off and autosave on (see ATTRIBUTION in
    // the file header). These must be set before MM_Rando_Init, because
    // COND_ID_HOOK evaluates its gate once, at ShipInit::InitAll time.
    CVarSetInteger(kChuDropsCVar, 0);
    CVarSetInteger(kJPGrottosCVar, 0);
    CVarSetInteger(kAutosaveCVar, 1);
    // Phase 4: the five Cheats keys start OFF whatever the operator's config
    // holds, so MM_Rando_Init arms none of them and the probe below measures
    // only what its own ShipInit::Init drive registers.
    for (const auto& probe : kCheatProbes) {
        CVarSetInteger(probe.key, 0);
    }
    // Phase 5: the same for the Skips & Speed-ups keys. They are not default-on
    // for MM, but the operator's config may hold them on (OoT's rando preset
    // writes them true), and a TU that is linked would then arm at MM_Rando_Init
    // and shrink the delta the probe measures.
    for (const auto& probe : SkipProbes()) {
        CVarSetInteger(probe.key, 0);
    }
    // Phase 6: the same for the enhancement keys (an operator config may hold
    // any of them on, and a linked TU would then arm at MM_Rando_Init).
    for (const auto& probe : EnhProbes()) {
        CVarSetInteger(probe.key, 0);
    }
    // Phase 7: the same for the Audio Editor keys.
    for (const auto& probe : AudioProbes()) {
        CVarSetInteger(probe.key, 0);
    }
    // Phase 8: the same for Free Look.
    for (const auto& probe : FreeLookProbes()) {
        CVarSetInteger(probe.key, 0);
    }

    // ---- Non-vacuity: every probed registry starts empty -------------------
    // If any of these is already populated, the "non-empty after" assertions
    // below prove nothing, so refuse to run rather than pass vacuously.
    RC_ASSERT(SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_ITEM00) == 0, 2,
              "ShouldActorInit[EN_ITEM00] was already populated before MM_Rando_Init — probe would be vacuous");
    RC_ASSERT(SettledCountForId<GameInteractor::OnOpenText>(CUSTOM_MESSAGE_ID) == 0, 2,
              "OnOpenText[0x004B] was already populated before MM_Rando_Init — probe would be vacuous");
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::BeforeMoonCrashSaveReset>() == 0, 2,
              "BeforeMoonCrashSaveReset was already populated before MM_Rando_Init — probe would be vacuous");
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::OnGameStateDrawFinish>() == 0, 2,
              "OnGameStateDrawFinish was already populated before MM_Rando_Init — probe would be vacuous");
    // #618 Phase 3: same non-vacuity contract for the two restored InitOTR
    // entries. Neither may have run before the production bring-up does.
    RC_ASSERT(MM_ExtensionRescan_CallCount() == 0, 2,
              "the ExtensionCache rescan already ran before MM_Rando_Init — probe would be vacuous (#618)");
    RC_ASSERT(MM_ExtensionRescan_Scanned() == 0, 2,
              "the MM-scoped ExtensionCache scan already completed before MM_Rando_Init — probe would be "
              "vacuous (#618)");
    RC_ASSERT(MM_ExtensionRescan_FlipbooksPatchedAfterScan() == 0, 2,
              "PlayerCustomFlipbooks_Patch already ran before MM_Rando_Init — probe would be vacuous (#618)");
    RC_ASSERT(SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_ZOG) == 0, 2,
              "OnActorInit[EN_ZOG] was already populated before MM_Rando_Init — probe would be vacuous (#1022)");

    // ---- The production bring-up -------------------------------------------
    // Once-only guarded internally; this row is the only caller in its process.
    //
    // TWO PHASES since ADR 0010 increment 2 (solver-inventory P13). The CORE
    // phase is everything in this row's coverage; the ASSET phase DEFERS with no
    // archives mounted, which is its entire purpose — the creation event runs
    // the core phase at OoT's file-create seam with MM never booted, and a latch
    // consumed there would rob the real MM boot of GfxPatcher, the tracker icons,
    // the asset-gated display-list copies and the ExtensionCache rescan.
    MM_Rando_Init();

    // P13, asserted rather than assumed: a full MM_Rando_Init in a ROM-free
    // process must leave the asset phase UNLATCHED. If this ever reads 1 here,
    // the split has silently collapsed back into one guard and the next real MM
    // boot after a creation event will skip every asset-dependent step.
    RC_ASSERT(MM_Rando_AssetPhaseLatched() == 0, 10,
              "the ASSET phase latched in a ROM-free process — a creation-time core bring-up would consume the "
              "real MM boot's asset work (ADR 0010 increment 2, P13)");

    // #618's trailing InitOTR pair belongs to the ASSET phase (the flipbook
    // patch self-latches, so it must not run before the ExtensionCache has seen
    // MM's archives). Driven by name here, which is what keeps this row's #618
    // coverage ROM-free: it is the SAME function the asset phase calls, at the
    // same point in the order.
    MM_ExtensionRescan_AfterArchiveMount();

    // ---- CustomItem::RegisterHooks (#516 critical) -------------------------
    // Without it CustomItem::Spawn's ACTOR_EN_ITEM00/ITEM00_NOTHING placeholder
    // is never swapped for the real item: every rando reward and every
    // cross-game arrival delivers nothing.
    const size_t enItem00Registrants = SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_ITEM00);
    RC_ASSERT(enItem00Registrants > 0, 3,
              "ShouldActorInit[EN_ITEM00] empty after MM_Rando_Init — CustomItem::RegisterHooks never ran, "
              "every randomized custom item is uncollectable (#516)");
    // Attribution integrity, not a feature assertion. With ChuDrops and
    // JPGrottos forced off, CustomItem's unconditional registrant is the only
    // one that may be here. A second means a competing gate came on -- most
    // likely EnItem00.cpp's IS_RANDO leg, if a shared-process `--test all`
    // ordering left saveType == SAVETYPE_RANDO -- and the probe above would no
    // longer be evidence that CustomItem ran. Fail loudly rather than pass on
    // someone else's registrant; if a fifth registrant is added deliberately,
    // update the ATTRIBUTION note in this file's header along with this count.
    RC_ASSERT(enItem00Registrants == 1, 7,
              "ShouldActorInit[EN_ITEM00] has a competing registrant — the CustomItem probe above is no longer "
              "attributable; see ATTRIBUTION in this file's header (#516)");

    // ---- CustomMessage::RegisterHooks (#516 critical) ----------------------
    // Its registrant is the only code that clears loadFromMessageTable and
    // loads the staged custom message; without it MM falls through to the real
    // vanilla message table and every rando/hint textbox renders entry 0x004B.
    RC_ASSERT(SettledCountForId<GameInteractor::OnOpenText>(CUSTOM_MESSAGE_ID) > 0, 4,
              "OnOpenText[0x004B] empty after MM_Rando_Init — CustomMessage::RegisterHooks never ran, "
              "every rando/hint textbox renders vanilla message 0x004B (#516)");

    // ---- RegisterSavingEnhancements (#516 Phase 2) -------------------------
    // Sole registrant of BeforeMoonCrashSaveReset. Also carries the OnSaveLoad
    // leg that seeds shipSaveContext.lastTimeLog — the seeder whose absence
    // injected a full Unix epoch into filePlaytime (#513).
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::BeforeMoonCrashSaveReset>() > 0, 5,
              "BeforeMoonCrashSaveReset empty after MM_Rando_Init — RegisterSavingEnhancements never ran, "
              "owl-save persistence and moon-crash cleanup are inert (#516)");

    // ---- RegisterAutosave (#516 Phase 2; ungateable by symbol name) --------
    // Sole registrant of OnGameStateDrawFinish, and the reason this row exists
    // at all: OoT's static twin makes the CI name-grep unattributable.
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::OnGameStateDrawFinish>() > 0, 6,
              "OnGameStateDrawFinish empty after MM_Rando_Init — RegisterAutosave never ran, "
              "MM has no periodic autosave and no autosave icon (#516)");

    // ---- #618 / #516 Phase 3: the last two InitOTR entries ------------------
    // Presence. Neither entry has a hook registry to probe, so what is probed
    // instead is that the bring-up REACHED them: the rescan completed, and the
    // flipbook patch was then called. Without the rescan MM's own archives are
    // absent from the shared ExtensionCache and every ResourceMgr_FileExists on
    // an MM path reads false — MM's HD gfxprint font and the static FD/Deku/
    // Goron faces both silently stay vanilla.
    RC_ASSERT(MM_ExtensionRescan_CallCount() == 1, 8,
              "MM_Rando_Init did not call the ExtensionCache rescan exactly once — MM's archives never reach "
              "the shared extension map, so every MM custom-asset check reads false (#618)");
    RC_ASSERT(MM_ExtensionRescan_Scanned() == 1, 8,
              "the MM-scoped ExtensionCache scan did not complete during MM_Rando_Init — MM custom assets are "
              "invisible to ResourceMgr_FileExists (#618)");
    // Order. PlayerCustomFlipbooks_PatchOnce latches sFacePatchState on its
    // FIRST call and never re-evaluates, so being called before the rescan would
    // permanently decide "no custom faces" from a cache that had not yet seen
    // MM's archives. This flag is only set when the patch ran with the scan
    // already complete, which is the ROM-free evidence of that ordering.
    RC_ASSERT(MM_ExtensionRescan_FlipbooksPatchedAfterScan() == 1, 9,
              "PlayerCustomFlipbooks_Patch did not run after the ExtensionCache rescan — its one-shot latch "
              "would fix MM's faces to vanilla against an unscanned cache (#618)");

    // ---- #1005-#1009: the five Cheats TUs -----------------------------------
    // Each probe turns ONE key on, drives ShipInit::Init(key), and requires its
    // registry to rise by exactly one. The baseline is read per probe, after the
    // previous probe's registrant is already counted, so the three
    // OnGameStateUpdate keys are attributed one at a time.
    for (const auto& probe : kCheatProbes) {
        const size_t before = probe.count();
        CVarSetInteger(probe.key, 1);
        ShipInit::Init(probe.key);
        const size_t after = probe.count();
        if (after != before + 1) {
            printf("[TEST] FAIL(11): %s went %zu -> %zu when %s was turned on, expected exactly +1 - its "
                   "Cheats/*.cpp registrar is not in the link (a plain 2ship_enh archive drops a TU whose only "
                   "anchor is a RegisterShipInitFunc), so the shared Cheats row does nothing in Majora's "
                   "Mask (#1005-#1009)\n",
                   probe.what, before, after, probe.key);
            ResetCheatKeys();
            return 11;
        }
    }
    ResetCheatKeys();

    // ---- #1022: HealingMikauAudio, the one unconditional Story TU -----------
    // Registers OnActorInit[EN_ZOG] at InitAll whatever any CVar says, and is the
    // only registrant of that bucket (SkipPushingMikau is ShouldActorInit).
    RC_ASSERT(SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_ZOG) == 1, 12,
              "OnActorInit[EN_ZOG] is not exactly one after MM_Rando_Init - StoryCutscenes/HealingMikauAudio.cpp "
              "is not in the link, so the Healing Mikau cutscene loses its Great Bay Coast BGM (#1022)");

    // ---- #1018-#1022: the Skips & Speed-ups TUs ------------------------------
    // Each probe turns ONE key on, drives ShipInit::Init(key), and requires every
    // registry the key's TUs write to to rise by exactly the table's count. The
    // baseline is read per probe after the previous key was put back to 0.
    for (const auto& probe : SkipProbes()) {
        std::vector<size_t> before;
        for (const auto& reg : probe.registries) {
            before.push_back(reg.count());
        }
        CVarSetInteger(probe.key, 1);
        ShipInit::Init(probe.key);
        for (size_t i = 0; i < probe.registries.size(); ++i) {
            const auto& reg = probe.registries[i];
            const size_t after = reg.count();
            if (after != before[i] + reg.expected) {
                printf("[TEST] FAIL(13): %s went %zu -> %zu when %s was turned on, expected exactly +%zu - the "
                       "Skips & Speed-ups TU(s) behind the \"%s\" row are not in the link (a plain 2ship_enh "
                       "archive drops a TU whose only anchor is a RegisterShipInitFunc), so the row does "
                       "nothing in Majora's Mask (#1018-#1022)\n",
                       reg.what, before[i], after, probe.key, reg.expected, probe.row);
                ResetSkipKeys();
                ResetCheatKeys();
                return 13;
            }
        }
        // Back to 0 before the next key so the buckets two keys share
        // (VB_START_CUTSCENE) are measured one key at a time.
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
    ResetSkipKeys();

    // ---- #1010-#1017: the eight enhancement TUs ------------------------------
    // Same drive as the Skips & Speed-ups loop above: ONE key on, ShipInit::Init
    // (key), every registry the key's TU writes to must move by exactly the
    // table's count (0 meaning "must not move"). The key is driven TWICE so a
    // registrar that stacks a registration per toggle shows up as a second delta.
    for (const auto& probe : EnhProbes()) {
        std::vector<size_t> before;
        for (const auto& reg : probe.registries) {
            before.push_back(reg.count());
        }
        CVarSetInteger(probe.key, 1);
        ShipInit::Init(probe.key);
        ShipInit::Init(probe.key);
        for (size_t i = 0; i < probe.registries.size(); ++i) {
            const auto& reg = probe.registries[i];
            const size_t after = reg.count();
            if (after != before[i] + reg.expected) {
                printf("[TEST] FAIL(14): %s went %zu -> %zu when %s was turned on and driven twice, expected "
                       "exactly +%zu - the enhancement TU behind the \"%s\" row is not in the link (a plain "
                       "2ship_enh archive drops a TU whose only anchor is a RegisterShipInitFunc), or it stacks "
                       "a registration per toggle, so the row does nothing or misbehaves in Majora's Mask "
                       "(#1010-#1017)\n",
                       reg.what, before[i], after, probe.key, reg.expected, probe.row);
                ResetEnhKeys();
                ResetSkipKeys();
                ResetCheatKeys();
                ResetAudioKeys();
                return 14;
            }
        }
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
    ResetEnhKeys();

    // ---- #1024-#1026: the three Audio Editor TUs -----------------------------
    // Same drive as the enhancement loop above: ONE key on, ShipInit::Init(key)
    // twice, every registry the key's TU writes to must move by exactly the
    // table's count.
    for (const auto& probe : AudioProbes()) {
        std::vector<size_t> before;
        for (const auto& reg : probe.registries) {
            before.push_back(reg.count());
        }
        CVarSetInteger(probe.key, 1);
        ShipInit::Init(probe.key);
        ShipInit::Init(probe.key);
        for (size_t i = 0; i < probe.registries.size(); ++i) {
            const auto& reg = probe.registries[i];
            const size_t after = reg.count();
            if (after != before[i] + reg.expected) {
                printf("[TEST] FAIL(15): %s went %zu -> %zu when %s was turned on and driven twice, expected "
                       "exactly +%zu - the Audio Editor TU behind the \"%s\" row is not in the link (a plain "
                       "2ship_enh archive drops a TU whose only anchor is a RegisterShipInitFunc), or it stacks "
                       "a registration per toggle, so the row does nothing or misbehaves in Majora's Mask "
                       "(#1024-#1026)\n",
                       reg.what, before[i], after, probe.key, reg.expected, probe.row);
                ResetAudioKeys();
                ResetEnhKeys();
                ResetSkipKeys();
                ResetCheatKeys();
                return 15;
            }
        }
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
    ResetAudioKeys();

    // ---- #1023: Free Look ------------------------------------------------------
    // Same drive as the Audio Editor loop above.
    for (const auto& probe : FreeLookProbes()) {
        std::vector<size_t> before;
        for (const auto& reg : probe.registries) {
            before.push_back(reg.count());
        }
        CVarSetInteger(probe.key, 1);
        ShipInit::Init(probe.key);
        ShipInit::Init(probe.key);
        for (size_t i = 0; i < probe.registries.size(); ++i) {
            const auto& reg = probe.registries[i];
            const size_t after = reg.count();
            if (after != before[i] + reg.expected) {
                printf("[TEST] FAIL(16): %s went %zu -> %zu when %s was turned on and driven twice, expected "
                       "exactly +%zu - Camera/FreeLook.cpp behind the \"%s\" row is not in the link (a plain "
                       "2ship_enh archive drops a TU whose only anchor is a RegisterShipInitFunc), or it stacks "
                       "a registration per toggle, so Free Look, Camera Distance and Camera Transition Speed do "
                       "nothing or misbehave in Majora's Mask (#1023)\n",
                       reg.what, before[i], after, probe.key, reg.expected, probe.row);
                ResetFreeLookKeys();
                ResetAudioKeys();
                ResetEnhKeys();
                ResetSkipKeys();
                ResetCheatKeys();
                return 16;
            }
        }
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
    ResetFreeLookKeys();

    printf("[TEST] mm-registrar-coverage: PASS (4 re-homed registrars populated their registries; the two "
           "Phase-3 ExtensionCache entries ran, in order; the 7 Cheats keys each armed one hook; the 5 "
           "Skips & Speed-ups keys each armed their TUs; the 9 enhancement keys of #1010-#1017 each armed "
           "theirs; the 3 Audio Editor keys of #1024-#1026 each armed theirs; Free Look "
           "of #1023 armed both its hooks)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
