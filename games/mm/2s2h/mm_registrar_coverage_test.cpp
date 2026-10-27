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
 * #1239 (W6-AUDIO-SFX) adds the Audio Editor's four other Sfx TUs, same drive:
 *   - gAudioEditor.MuteCarpenterSfx: ShouldActorInit keyed on ACTOR_OBJ_SOUND
 *     (Audio/Sfx/MuteCarpenterSfx.cpp; MM row in SoH's Audio Options block).
 *   - gAudioEditor.ChildGoronCry: VB_PLAY_GORON_CHILD_CRY (ovl_En_Gk; MM row).
 *   - gAudioEditor.DisableNaviCallAudio: VB_PLAY_TATL_CALL_AUDIO (z_parameter.c).
 *     SoH's Navi call row: MM's Tatl call converged onto it (kConvergedKeys).
 *   - gAudioEditor.LinkVoiceFreqMultiplier: VB_LINK_VOICE_PITCH_MULTIPLIER
 *     (z_actor.c). SoH's float slider, translated onto MM's pitch hook
 *     (kTranslatedKeys): any value but 1.0 arms it, so this probe drives the
 *     float (onFloat) instead of an integer 1, and clears the key afterwards.
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
 *     gEnhancements.Camera.DebugCam.Enable (linked since W6-CAM-GYRO, which drives it beside Free Look).
 *   - OnCameraChangeModeFlags, unkeyed (z_camera.c, Camera_ChangeMode): the
 *     per-mode "may free-look" reset.
 *
 * PHASE 9 (#1030-#1034) -- THE FIVE KEY-GAP TUs. Accessibility/NoScreenFlashForEnemyKill,
 * Graphics/3DItemDrops, Cutscenes/MiscInteractions/* (25 TUs), Songs/SkipScarecrowSong and
 * Equipment/ArrowCycle register only through a file-scope RegisterShipInitFunc
 * keyed on the key SoH's row writes (PR #1046 moved MM's readers onto it:
 * gSettings.A11yNoScreenFlashForFinishingBlow, gEnhancements.NewDrops,
 * gEnhancements.TimeSavers.SkipMiscInteractions, gEnhancements.InstantScarecrow,
 * gEnhancements.BowArrowCycle), in the plain `2ship_enh` archive, so the linker
 * dropped all 29 and the five rows stayed dead even once mapped. Same probe as
 * Phases 5-8: the key ON plus ShipInit::Init(key), driven twice, must raise every
 * registry the TU(s) write to by EXACTLY the count the table names. Each key is
 * read by these TUs alone (BenMenu.cpp only draws it; ChuDrops.cpp reads
 * gEnhancements.NewDrops inline and registers nothing on it), so the delta is
 * attributable. Nothing a TU defines is named here (a reference would un-elide it).
 *   - Skip Misc Interactions: the 25 TUs' registrations are summed per bucket in the table
 *     below, each bucket attributed to its TU(s) by name. Several TUs share a bucket
 *     (VB_START_CUTSCENE has eight of them), so one dropped TU leaves its bucket short.
 *
 * PHASE 10 (#1035, #1036, #1038) -- THE THREE REMAINING KEY-GAP TUs. Equipment/SkipMagicArrowEquip,
 * DifficultyOptions/PermanentHeartLoss and Restorations/N64WeirdFrames/N64WeirdFrames have the shape of
 * Phase 9 (PR #1046 moved their readers onto gEnhancements.SkipArrowAnimation, gEnhancements.PermanentHeartLoss
 * and gEnhancements.N64WeirdFrames; each registers only through a file-scope RegisterShipInitFunc in the plain
 * `2ship_enh` archive) and ride the same table. Each key is read by its one TU alone, so the delta is
 * attributable.
 *   - Skip Magic Arrow Equip Animation: OnKaleidoUpdate, unkeyed (z_kaleido_scope_NES.c, through
 *     MM_GameHooks_ExecuteOnKaleidoUpdate).
 *   - Permanent Heart Loss: OnActorUpdate[ACTOR_PLAYER].
 *   - N64 Weird Frames: VB_LOAD_PLAYER_ANIMATION_FRAME (z_skelanime.c, AnimTaskQueue_AddLoadPlayerFrame).
 *
 * PHASE 11 (#1123) -- SKIP BOSS INTRODUCTIONS. Cutscenes/SkipEnemyCutscenes reads SoH's
 * gEnhancements.TimeSavers.SkipCutscene.BossIntro (it read MM's own Cutscenes.SkipEnemyCutscenes, which nothing in
 * the single exe writes) and registers only through three file-scope RegisterShipInitFuncs on that key. Same table
 * as Phase 9; the key is read by this one TU alone in games/mm, so the delta is attributable.
 *
 * PHASE 12 (#1215, #1221) -- THE RANDO-FORCED GATES. Four 2s2h/Enhancements TUs register a `CVAR || IS_RANDO` gate
 * through a file-scope RegisterShipInitFunc only: Cheats/UnbreakableRazorSword, Cycle/EndOfCycle,
 * Cycle/OceansideWalletAnyDay and Cycle/OceansideSpiderHouseSquatter. Upstream forces each on for every rando file;
 * without them a rando file runs the vanilla behaviour. #1224 ported their IS_RANDO halves while the TUs were
 * unlinked; W6-CYCLE-CHEATS (#1236) links them whole, each CVar drawn by an MM row, and retires the port, so each gate
 * must still register exactly once. A row-on leg sits between the two: with IS_RANDO off and a gate's own key on, the
 * gate registers once and answers as under rando (the Razor restore's count only), and with both off it is gone.
 * EndOfCycle's Razor restore is MMCycleRazorSword's (mm_cycle_keep_items_test.cpp). This phase probes the other three
 * through the production arm drive (saveType SAVETYPE_RANDO, then ShipInit::Init("IS_RANDO"), what OnSaveLoadHandler
 * runs on every MM load): disarmed first (the vanilla control and the non-vacuity baseline), then armed, where each
 * bucket must rise by EXACTLY one and the answer must flip. The rando-only registrants armed from OnFileLoad (the
 * OfferGetItem catch-all, the OnFlagSet check hand-off) are not ShipInit registrars, so they cannot move these deltas.
 * Nothing those TUs define is named here (a reference would un-elide them).
 *   - ShouldVanillaBehavior[VB_LOWER_RAZOR_SWORD_DURABILITY] (#1215, the Razor's ~100-hit break, z_player.c
 *     func_8083FFEC): the verdict is "do not lower" under rando, "lower" on a vanilla file.
 *   - ShouldVanillaBehavior[VB_GIVE_ITEM_FROM_OFFER] (#1221 row 1, z_actor.c MM_Actor_OfferGetItem): a stand-in
 *     EnSth (STH_TYPE_OCEANSIDE_SPIDER_HOUSE_GREET) offering GI_RUPEE_PURPLE, the day-2 reward, with the wallet flag
 *     clear sets WEEKEVENTREG_RECEIVED_OCEANSIDE_WALLET_UPGRADE (RC_OCEAN_SPIDER_HOUSE_WALLET's flag) and offers the
 *     next wallet under rando; a vanilla file keeps the rupee and the flag stays clear.
 *   - OnFlagSet, unkeyed (#1221 row 2, Mikau's grave z_en_sekihi.c): BUYER_MOVED_IN set before the reward is
 *     collected is cleared again under rando (it stays set on a vanilla file), and collecting the reward sets it.
 * The whole SaveContext is saved first and copied back, and the "IS_RANDO" drive re-runs under the restored saveType,
 * so the phase is order-free.
 *
 * THE W6 PHASES (#1233) -- ONE LIST, ONE CODE. The Majora's Mask enhancement settings lanes each link a batch of
 * RegisterShipInitFunc-only TUs, the Phase 5-11 shape. Instead of a phase and a failure code each, a lane registers
 * one SkipKeyProbe table in the self-registering W6 list (W6Phases, above Phase 4); ZeroW6PhaseKeys holds every key
 * off before MM_Rando_Init, and RunW6PhaseList drives each table as Phases 5-9 do, failing with 18 and naming the
 * phase and key. A synthetic phase whose key arms nothing is driven through the same code first, on every run, and
 * must come back 18. Each table attributes its TUs by registry content, never by a symbol they define.
 *
 * W6-PLAYER'S BOMBCHU DROPS RULE (#1236, review H1) -- FAIL 22. The W6-PLAYER table shows that ChuDrops.cpp
 * registers; this leg shows what its registrant decides. The Bombchu count is a shared PROGRESSION quantity
 * (src/common/shared_items.h), so MM's Enable Bombchu Drops may convert a Bomb drop only while
 * INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU already holds, Ship of Harkinian's first clause for its own Enable
 * Bombchu Drops. 2Ship gives every Bomb drop the conversion roll whatever Link carries, and the pickup's Item_Give
 * would hand Termina the Bombchu item and Hyrule, through the pool, the count and the item before the seed placed
 * them. The leg arms the key, takes the ONE ShouldActorInit[EN_ITEM00] registrant arming added, and calls it on a
 * stand-in Bomb drop (ITEM00_BOMBS_A) over a zeroed stand-in play state (without one the hook's play-state guard
 * returns first and both cases would pass vacuously), with Ship_Random seeded so the first roll keeps the Bomb
 * (>= 50; 2Ship converts below 50) and the second roll differs. The roll drawn after a call then says how many rolls
 * the call took:
 *   - no Bombchus held: no roll, and the Bomb drop is kept;
 *   - Bombchus held: exactly one roll, so the drop is up for conversion, and the Bomb is kept because the roll said
 *     so. This is also the leg's non-vacuity control: a stand-in the hook never reached would take no roll here.
 * The converting roll spawns a CustomItem through MM_Actor_Spawn, which needs a live actor context, so the leg never
 * takes it; a Bombchu appearing is the playtest's. Nothing ChuDrops.cpp defines is named here.
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
#include "2s2h/ShipUtils.h"
#include "mm_game_hooks.h"

#include <cstring>
#include <memory>
#include <random>

extern "C" {
// Phase 12 (#1215, #1221): the rando arm drive and the gates' flags.
#include "variables.h"
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
// W6-CYCLE-CHEATS: z_player.c's Deku-glide distance pair, which Longer Deku Flower Glide rewrites.
extern f32 D_8085D958[2];
}

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

// ---- W6 phases (#1233): the self-registering phase list -------------------
// Each W6 lane that links MM enhancement TUs adds ONE probe table (Phase 5's SkipKeyProbe shape: a key, its row, and
// per registry the TUs' summed registration count) and ONE static registration, at the anchor its brief names:
//
//     const std::vector<SkipKeyProbe>& TimesaverProbes() { static const std::vector<SkipKeyProbe> p = { ... }; ... }
//     RegisterW6Phase sTimesaverPhase{ "W6-TIMESAVERS (#1236)", TimesaverProbes };
//
// and never edits the runner: ZeroW6PhaseKeys holds every registered key off before MM_Rando_Init, and RunW6Phases
// drives each table exactly as Phases 5-9 do (key on, ShipInit::Init twice, exact deltas, key off), failing with one
// code, 18, that names the phase and the key. A table registered before its TUs link is red, which is the lane's lock.
struct SkipKeyProbe; // Phase 5's probe shape, defined below.

struct W6Phase {
    const char* name;
    const std::vector<SkipKeyProbe>& (*probes)();
};

std::vector<W6Phase>& W6Phases() {
    static std::vector<W6Phase> phases;
    return phases;
}

struct RegisterW6Phase {
    RegisterW6Phase(const char* name, const std::vector<SkipKeyProbe>& (*probes)()) {
        W6Phases().push_back(W6Phase{ name, probes });
    }
};

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

// ---- W6-CYCLE-CHEATS (#1236, #1240): MM's cheats and Time Cycle options --------
// One row per key the hosted rows write (SohGui/HostedMm/SohMenuHostedMmCycleCheats.cpp): the registry its TU puts a
// hook in when the key turns on, with IS_RANDO off. The nine TUs are named only in the text; nothing they define is.
//   - Longer Deku Flower Glide registers no hook: its registrar rewrites z_player.c's glide-distance pair
//     (D_8085D958, 600/960 vanilla), so its "registry" is that pair being lifted past vanilla, 0 or 1.
//   - Do Not Reset Rupees / Consumables expect +0: EndOfCycle.cpp holds both restores off in the single exe (the
//     combo's cycle hooks and the frozen Keep Items on Cycle Reset decide them), so a hand-set key arms nothing.
//   - Infinite Consumables rides Cheats/Infinite.cpp, linked since #1005-#1009; only its row is new.
struct CycleCheatRow {
    const char* key;
    const char* row;
    const char* what;
    size_t (*count)();
    size_t expected;
};

size_t CycleCheatGlideLifted() {
    return D_8085D958[0] > 960.0f ? 1u : 0u;
}

#define CYCLE_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }
#define CYCLE_UNKEYED_COUNT(hook) []() { return SettledUnkeyedCount<GameInteractor::hook>(); }

const CycleCheatRow kCycleCheatRows[] = {
    { "gCheats.InfiniteConsumables", "MM Consumables", "OnGameStateUpdate (Cheats/Infinite.cpp)",
      CYCLE_UNKEYED_COUNT(OnGameStateUpdate), 1 },
    { "gCheats.UnrestrictedItems", "MM Unrestricted Items", "VB_ITEM_BE_RESTRICTED (Cheats/UnrestrictedItems.cpp)",
      CYCLE_VB_COUNT(VB_ITEM_BE_RESTRICTED), 1 },
    { "gCheats.LongerFlowerGlide", "Longer Deku Flower Glide",
      "z_player.c's glide distance lifted (Cheats/LongerFlowerGlide.cpp)", CycleCheatGlideLifted, 1 },
    { "gCheats.ElegyAnywhere", "Elegy of Emptiness Anywhere", "VB_ELEGY_CHECK_SCENE (Cheats/ElegyAnywhere.cpp)",
      CYCLE_VB_COUNT(VB_ELEGY_CHECK_SCENE), 1 },
    { "gCheats.TempleTimeStop", "Stop Time in Dungeons", "AfterRoomSceneCommands, unkeyed (Cheats/TimeStop.cpp)",
      CYCLE_UNKEYED_COUNT(AfterRoomSceneCommands), 1 },
    { "gCheats.UnbreakableRazorSword", "Unbreakable Razor Sword",
      "VB_LOWER_RAZOR_SWORD_DURABILITY (Cheats/UnbreakableRazorSword.cpp)",
      CYCLE_VB_COUNT(VB_LOWER_RAZOR_SWORD_DURABILITY), 1 },
    { "gEnhancements.Cycle.DoNotResetBottleContent", "Do Not Reset Bottle Contents",
      "AfterEndOfCycleSave, unkeyed (Cycle/EndOfCycle.cpp)", CYCLE_UNKEYED_COUNT(AfterEndOfCycleSave), 1 },
    { "gEnhancements.Cycle.DoNotResetRazorSword", "Do Not Reset Razor Sword",
      "AfterEndOfCycleSave, unkeyed (Cycle/EndOfCycle.cpp)", CYCLE_UNKEYED_COUNT(AfterEndOfCycleSave), 1 },
    { "gEnhancements.Cycle.DoNotResetTimeSpeed", "Do Not Reset Time Speed",
      "AfterEndOfCycleSave, unkeyed (Cycle/EndOfCycle.cpp)", CYCLE_UNKEYED_COUNT(AfterEndOfCycleSave), 1 },
    { "gEnhancements.Cycle.DoNotResetRupees", "(no row: the combo keeps the shared rupees)",
      "AfterEndOfCycleSave, unkeyed (Cycle/EndOfCycle.cpp, held off)", CYCLE_UNKEYED_COUNT(AfterEndOfCycleSave), 0 },
    { "gEnhancements.Cycle.DoNotResetConsumables", "(no row: the combo and Keep Items on Cycle Reset decide)",
      "AfterEndOfCycleSave, unkeyed (Cycle/EndOfCycle.cpp, held off)", CYCLE_UNKEYED_COUNT(AfterEndOfCycleSave), 0 },
    { "gEnhancements.Cycle.KeepExpressMail", "Keep Express Mail", "VB_MSG_SCRIPT_DEL_ITEM (Cycle/KeepExpressMail.cpp)",
      CYCLE_VB_COUNT(VB_MSG_SCRIPT_DEL_ITEM), 1 },
    { "gEnhancements.Cycle.StopOceansideSpiderHouseSquatter", "Stop Oceanside Spider House Squatter",
      "OnFlagSet, unkeyed (Cycle/OceansideSpiderHouseSquatter.cpp)", CYCLE_UNKEYED_COUNT(OnFlagSet), 1 },
    { "gEnhancements.Cycle.OceansideWalletAnyDay", "Oceanside Wallet Any Day",
      "VB_GIVE_ITEM_FROM_OFFER (Cycle/OceansideWalletAnyDay.cpp)", CYCLE_VB_COUNT(VB_GIVE_ITEM_FROM_OFFER), 1 },
};

// The rows above in Phase 5's probe shape. Defined after Phase 12 (SkipKeyProbe is complete there).
const std::vector<SkipKeyProbe>& CycleCheatProbes();
RegisterW6Phase sCycleCheatPhase{ "W6-CYCLE-CHEATS (#1236, #1240)", CycleCheatProbes };

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
    // #1239: 0 = an integer toggle, driven to 1 and back to 0. Otherwise a float key (SoH's voice-pitch slider),
    // driven to this value and cleared afterwards, since an integer write would leave a key its reader ignores.
    float onFloat = 0.0f;
    // W6-CAM-GYRO: keys the W6 drive holds on (1) around this probe, for a TU whose registrar ANDs two keys (the speed
    // modifiers' master and walk / swim enables) or a probe that measures one TU beside another (the debug camera
    // beside Free Look). Set and driven before the baseline is read, put back to 0 and driven after. Empty for every
    // other table, which the drive then treats exactly as before.
    std::vector<const char*> alsoOn = {};
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

// ---- W6-TIMESAVERS (#1236, #1240): MM's dialogue, time-saver and pause-save TUs
// -------------------------------------- One probe per MM key a hosted row writes, plus the shared Debug Mode key, the
// third of Fixes/BgmReplay.cpp's gates (driven here so ZeroW6PhaseKeys holds it off and the other two attribute
// BgmReplay exactly). Per registry, the TUs: Dialogue/FastBankSelection.cpp, Dialogue/AutoBombersCode.cpp,
// Timesavers/SwampBoatSkip.cpp, GalleryTwofer.cpp, MarineLabHP.cpp, DampeDiggingSkip.cpp (two actor buckets),
// FasterSceneTransitions.cpp, AutoBankDeposit.cpp, Minigames/SkipPowderKegCertification.cpp (its OnOpenText leg is
// !IS_RANDO; this phase runs on a vanilla save), Minigames/SkipBalladOfWindfish.cpp, Saving/PauseSave.cpp, and
// Fixes/BgmReplay.cpp under all three of its keys. Fast Text has no TU (z_message.c reads it), so it has no probe.
#define TS_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }
#define TS_ACTOR_UPDATE_COUNT(id) []() { return SettledCountForId<GameInteractor::OnActorUpdate>(id); }

const std::vector<SkipKeyProbe>& TimesaverProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gEnhancements.Dialogue.FastBankSelection",
          "Fast Bank Selection",
          { { "OnActorUpdate[EN_GINKO_MAN] (Dialogue/FastBankSelection.cpp)", TS_ACTOR_UPDATE_COUNT(ACTOR_EN_GINKO_MAN),
              1 } } },
        { "gEnhancements.Dialogue.AutoBombersCode",
          "Auto Bombers' Code",
          { { "OnActorUpdate[EN_BOMBERS2] (Dialogue/AutoBombersCode.cpp)", TS_ACTOR_UPDATE_COUNT(ACTOR_EN_BOMBERS2),
              1 } } },
        { "gEnhancements.Timesavers.SwampBoatSpeed",
          "Swamp Boat Timesaver",
          { { "OnActorUpdate[BG_INGATE] (Timesavers/SwampBoatSkip.cpp)", TS_ACTOR_UPDATE_COUNT(ACTOR_BG_INGATE),
              1 } } },
        { "gEnhancements.Timesavers.GalleryTwofer",
          "Shooting Gallery Both Rewards",
          { { "OnFlagSet, unkeyed (Timesavers/GalleryTwofer.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnFlagSet>(); }, 1 } } },
        { "gEnhancements.Timesavers.MarineLabHP",
          "Fast Marine Lab Fish",
          { { "OnActorInit[EN_FISH2] (Timesavers/MarineLabHP.cpp)",
              []() { return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_FISH2); }, 1 } } },
        { "gEnhancements.Timesavers.DampeDiggingSkip",
          "Fast Dampe Flame Digging",
          { { "OnActorUpdate[EN_TK] (Timesavers/DampeDiggingSkip.cpp)", TS_ACTOR_UPDATE_COUNT(ACTOR_EN_TK), 1 },
            { "OnActorUpdate[EN_BIGPO] (Timesavers/DampeDiggingSkip.cpp)", TS_ACTOR_UPDATE_COUNT(ACTOR_EN_BIGPO),
              1 } } },
        { "gEnhancements.Timesavers.FasterSceneTransitions",
          "Faster Scene Transitions",
          { { "VB_SETUP_TRANSITION (Timesavers/FasterSceneTransitions.cpp)", TS_VB_COUNT(VB_SETUP_TRANSITION), 1 },
            { "VB_PLAY_SCENE_SEQUENCE (Fixes/BgmReplay.cpp)", TS_VB_COUNT(VB_PLAY_SCENE_SEQUENCE), 1 } } },
        { "gEnhancements.Timesavers.PowderKegCertification",
          "Skip Powder Keg Certification",
          { { "VB_EXEC_MSG_EVENT (Minigames/SkipPowderKegCertification.cpp)", TS_VB_COUNT(VB_EXEC_MSG_EVENT), 1 },
            { "OnOpenText[0x0C86] (Minigames/SkipPowderKegCertification.cpp)",
              []() { return SettledCountForId<GameInteractor::OnOpenText>(0x0C86); }, 1 } } },
        { "gEnhancements.Timesavers.SkipBalladOfWindfish",
          "Skip Ballad of Windfish",
          { { "VB_BALLAD_PLAYED_FORM (Minigames/SkipBalladOfWindfish.cpp)", TS_VB_COUNT(VB_BALLAD_PLAYED_FORM), 1 } } },
        { "gEnhancements.Timesavers.AutoBankDeposit",
          "Auto Bank Deposit",
          { { "VB_DISCARD_EXCESS_RUPEES (Timesavers/AutoBankDeposit.cpp)", TS_VB_COUNT(VB_DISCARD_EXCESS_RUPEES),
              1 } } },
        { "gEnhancements.Saving.PauseSave",
          "Pause Menu Save",
          { { "VB_SAVE_ON_B_BUTTON_IN_PAUSE_MENU (Saving/PauseSave.cpp)",
              TS_VB_COUNT(VB_SAVE_ON_B_BUTTON_IN_PAUSE_MENU), 1 },
            { "VB_PLAY_SCENE_SEQUENCE (Fixes/BgmReplay.cpp)", TS_VB_COUNT(VB_PLAY_SCENE_SEQUENCE), 1 } } },
        { "gDeveloperTools.DebugEnabled",
          "Debug Mode (BgmReplay's third gate)",
          { { "VB_PLAY_SCENE_SEQUENCE (Fixes/BgmReplay.cpp)", TS_VB_COUNT(VB_PLAY_SCENE_SEQUENCE), 1 } } },
    };
    return probes;
}

RegisterW6Phase sTimesaverPhase{ "W6-TIMESAVERS (#1236)", TimesaverProbes };

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

// ---- W6-MASKS-SONGS (#1236, #1240): MM's Masks and Ocarina TUs ----------------
// One probe per key; each TU registers only through a file-scope RegisterShipInitFunc. Attributed by registry content:
// every bucket is keyed on a VB id, an actor id, a limb or a pause page, or is an unkeyed registry no other W6 key
// arms, and the delta is measured with every other key off, so only the TU behind the probed key can move it (the
// unconditional registrants on the same buckets, Rando/ForeignMaskMM.cpp's head draw and player update among them, are
// in both counts). Pause Owl Warp's TU was already linked (z_kaleido_*.c calls its extern "C" accessor); its probe is
// held here so a link change cannot drop its registrar unseen now that a row writes its key.
#define W6_MASKS_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }

constexpr const char* kW6ZoraEggCountCVar = "gEnhancements.Songs.ZoraEggCount";

const std::vector<SkipKeyProbe>& W6MasksSongsProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gEnhancements.Masks.EquipWhileSwimming",
          "Equippable While Swimming",
          { { "VB_USE_ITEM_CONSIDER_ITEM_ACTION (Masks/EquipWhileSwimming.cpp)",
              W6_MASKS_VB_COUNT(VB_USE_ITEM_CONSIDER_ITEM_ACTION), 1 },
            { "VB_DISABLE_ITEM_UNDERWATER (Masks/EquipWhileSwimming.cpp)",
              W6_MASKS_VB_COUNT(VB_DISABLE_ITEM_UNDERWATER), 1 } } },
        { "gEnhancements.Masks.BlastMaskKeg",
          "Blast Mask Has Powder Keg Force",
          { { "ShouldActorInit[ACTOR_EN_BOM] (Masks/BlastMaskKeg.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_BOM); }, 1 } } },
        { "gEnhancements.Masks.FastTransformation",
          "Fast Transformation",
          { { "VB_PREVENT_MASK_TRANSFORMATION_CS (Masks/FastTransformation.cpp)",
              W6_MASKS_VB_COUNT(VB_PREVENT_MASK_TRANSFORMATION_CS), 1 } } },
        { "gEnhancements.Masks.FierceDeitysAnywhere",
          "Fierce Deity's Mask Anywhere",
          { { "VB_DISABLE_FD_MASK (Masks/FierceDeityAnywhere.cpp)", W6_MASKS_VB_COUNT(VB_DISABLE_FD_MASK), 1 },
            { "VB_DAMAGE_EFFECT (Masks/FierceDeityAnywhere.cpp)", W6_MASKS_VB_COUNT(VB_DAMAGE_EFFECT), 1 },
            { "VB_BE_NEAR_DOOR (Masks/FierceDeityAnywhere.cpp)", W6_MASKS_VB_COUNT(VB_BE_NEAR_DOOR), 1 },
            { "ShouldActorUpdate[ACTOR_EN_BIGOKUTA] (Masks/FierceDeityAnywhere.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorUpdate>(ACTOR_EN_BIGOKUTA); }, 1 },
            { "ShouldActorInit[ACTOR_EN_CLEAR_TAG] (Masks/FierceDeityAnywhere.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_CLEAR_TAG); }, 1 } } },
        // UpdatePersistentMasksState unregisters its two id-keyed hooks before it registers them again, so driving the
        // key twice settles at +1 on each; with the key off it unregisters them and clears the internal .State key.
        { "gEnhancements.Masks.PersistentBunnyHood.Enabled",
          "Persistent Bunny Hood",
          { { "OnPlayerPostLimbDraw[PLAYER_LIMB_HEAD] (Masks/PersistentMasks.cpp)",
              []() { return SettledCountForId<GameInteractor::OnPlayerPostLimbDraw>(PLAYER_LIMB_HEAD); }, 1 },
            { "BeforeKaleidoDrawPage[PAUSE_MASK] (Masks/PersistentMasks.cpp)",
              []() { return SettledCountForId<GameInteractor::BeforeKaleidoDrawPage>(PAUSE_MASK); }, 1 },
            { "VB_CONSIDER_BUNNY_HOOD_EQUIPPED (Masks/PersistentMasks.cpp)",
              W6_MASKS_VB_COUNT(VB_CONSIDER_BUNNY_HOOD_EQUIPPED), 1 },
            { "VB_GET_ITEM_ACTION_FROM_MASK (Masks/PersistentMasks.cpp)",
              W6_MASKS_VB_COUNT(VB_GET_ITEM_ACTION_FROM_MASK), 1 } } },
        { "gEnhancements.Masks.NoBlastMaskCooldown",
          "No Blast Mask Cooldown",
          { { "VB_SET_BLAST_MASK_COOLDOWN_TIMER (Masks/NoBlastMaskCooldown.cpp)",
              W6_MASKS_VB_COUNT(VB_SET_BLAST_MASK_COOLDOWN_TIMER), 1 } } },
        { "gEnhancements.Masks.GoronRollingIgnoresMagic",
          "Goron Rolling Ignores Magic",
          { { "VB_GORON_ROLL_CONSUME_MAGIC (Masks/GoronRollingIgnoresMagic.cpp)",
              W6_MASKS_VB_COUNT(VB_GORON_ROLL_CONSUME_MAGIC), 1 },
            { "VB_GORON_ROLL_DISABLE_SPIKE_MODE (Masks/GoronRollingIgnoresMagic.cpp)",
              W6_MASKS_VB_COUNT(VB_GORON_ROLL_DISABLE_SPIKE_MODE), 1 },
            { "VB_GORON_ROLL_INCREASE_SPIKE_LEVEL (Masks/GoronRollingIgnoresMagic.cpp)",
              W6_MASKS_VB_COUNT(VB_GORON_ROLL_INCREASE_SPIKE_LEVEL), 1 } } },
        { "gEnhancements.Masks.GoronRollingFastSpikes",
          "Goron Rolling Fast Spikes",
          { { "VB_GORON_ROLL_INCREASE_SPIKE_LEVEL (Masks/GoronRollingFastSpikes.cpp)",
              W6_MASKS_VB_COUNT(VB_GORON_ROLL_INCREASE_SPIKE_LEVEL), 1 } } },
        { "gEnhancements.Songs.PauseOwlWarp",
          "Pause Owl Warp",
          { { "OnKaleidoUpdate, unkeyed (Songs/PauseOwlWarp.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnKaleidoUpdate>(); }, 1 } } },
        { "gEnhancements.Songs.EnableSunsSong",
          "Enable Sun's Song",
          { { "VB_SONG_AVAILABLE_TO_PLAY (Songs/EnableSunsSong.cpp)", W6_MASKS_VB_COUNT(VB_SONG_AVAILABLE_TO_PLAY),
              1 } } },
        // A slider whose default (7, every egg) is the one value that disarms it: ZoraEggCount.cpp arms while the key
        // is not 7, so the drive's "off" (0) would arm it too. This count first puts a 0 back to the default (cleared,
        // so 7) and re-drives it, then reads the bucket: 0 before the drive, +1 while the drive holds the key at 1 (one
        // egg), and the key is left cleared and disarmed by every count after the drive. Without the TU in the link
        // nothing arms and the probe is red.
        { kW6ZoraEggCountCVar,
          "Zora Eggs for Bossa Nova",
          { { "OnActorUpdate[ACTOR_EN_MK] (Songs/ZoraEggCount.cpp)",
              []() {
                  if (CVarGetInteger(kW6ZoraEggCountCVar, 7) == 0) {
                      CVarClear(kW6ZoraEggCountCVar);
                      ShipInit::Init(kW6ZoraEggCountCVar);
                  }
                  return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_EN_MK);
              },
              1 } } },
        { "gEnhancements.Songs.FasterSongPlayback",
          "Faster Song Playback",
          { { "OnActorUpdate[ACTOR_PLAYER] (Songs/FasterSongPlayback.cpp)",
              []() { return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_PLAYER); }, 1 },
            { "ShouldActorInit[ACTOR_EFF_CHANGE] (Songs/FasterSongPlayback.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EFF_CHANGE); }, 1 },
            { "VB_ELEGY_STATUE_FADE_IN_OUT (Songs/FasterSongPlayback.cpp)",
              W6_MASKS_VB_COUNT(VB_ELEGY_STATUE_FADE_IN_OUT), 1 } } },
        { "gEnhancements.Songs.SkipSoaringCutscene",
          "Skip Soaring Cutscene",
          { { "ShouldActorInit[ACTOR_EN_TEST7] (Songs/SkipSoaringCutscene.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_TEST7); }, 1 } } },
        { "gEnhancements.Player.UnderwaterOcarina",
          "Underwater Ocarina",
          { { "VB_DISABLE_ITEM_UNDERWATER (Player/UnderwaterOcarina.cpp)",
              W6_MASKS_VB_COUNT(VB_DISABLE_ITEM_UNDERWATER), 1 } } },
    };
    return probes;
}

RegisterW6Phase sW6MasksSongsPhase{ "W6-MASKS-SONGS (#1236, #1240)", W6MasksSongsProbes };

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

// ---- W6-PLAYER (#1236): MM's Player, Equipment and PlayerActions TUs ---------
// One probe per key; each TU registers only through a file-scope RegisterShipInitFunc. Attributed by registry
// content: every bucket below is keyed on a VB id, an actor id or a text id, and the delta is measured with every
// other key off, so only the TU behind the probed key can move it. ShouldActorInit[EN_ITEM00] also holds
// CustomItem's unconditional registrant, JPGrottos' (forced off) and the rando EnItem00 leg: the harness configuration
// forces ChuDrops off before MM_Rando_Init (kChuDropsCVar) and ZeroW6PhaseKeys does again, so the exactly-one probe
// (FAIL 7) runs honest; this phase turns ChuDrops on, measures +1 and clears it.
#define W6_PLAYER_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }

constexpr const char* kW6PlayerClimbSpeedCVar = "gEnhancements.Player.ClimbSpeed";

const std::vector<SkipKeyProbe>& W6PlayerProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gEnhancements.Player.FastFlowerLaunch",
          "Fast Deku Flower Launch",
          { { "OnActorUpdate[ACTOR_PLAYER] (Player/FastFlowerLaunch.cpp)",
              []() { return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_PLAYER); }, 1 } } },
        { "gEnhancements.Player.InfiniteDekuHopping",
          "Infinite Deku Hopping",
          { { "VB_DEKU_LINK_SPIN_ON_LAST_HOP (Player/InfiniteDekuHopping.cpp)",
              W6_PLAYER_VB_COUNT(VB_DEKU_LINK_SPIN_ON_LAST_HOP), 1 },
            { "VB_APPLY_AIR_CONTROL (Player/InfiniteDekuHopping.cpp)", W6_PLAYER_VB_COUNT(VB_APPLY_AIR_CONTROL),
              1 } } },
        { "gEnhancements.Player.FierceDeityPutaway",
          "Fierce Deity Putaway",
          { { "VB_SHOULD_PUTAWAY (Player/FierceDeityPutaway.cpp)", W6_PLAYER_VB_COUNT(VB_SHOULD_PUTAWAY), 1 },
            { "VB_FD_ALWAYS_WIELD_SWORD (Player/FierceDeityPutaway.cpp)", W6_PLAYER_VB_COUNT(VB_FD_ALWAYS_WIELD_SWORD),
              1 } } },
        // A slider: ClimbSpeed.cpp arms only past 1x (`CVAR > 1`), and the W6 drive turns a key "on" with 1. So this
        // count, read while the drive holds the key at 1, first re-drives it at 2x (the slider's lowest armed value),
        // and reads the bucket there; the drive's own "off" (0) then disarms it. Before the drive the key is 0 and the
        // count is a plain read. Without the TU in the link the re-drive arms nothing and the probe is red.
        { kW6PlayerClimbSpeedCVar,
          "MM Vine/Ladder Climb Speed",
          { { "VB_SET_CLIMB_SPEED at 2x (Player/ClimbSpeed.cpp)",
              []() {
                  if (CVarGetInteger(kW6PlayerClimbSpeedCVar, 0) == 1) {
                      CVarSetInteger(kW6PlayerClimbSpeedCVar, 2);
                      ShipInit::Init(kW6PlayerClimbSpeedCVar);
                      ShipInit::Init(kW6PlayerClimbSpeedCVar);
                  }
                  return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_SET_CLIMB_SPEED);
              },
              1 } } },
        { "gEnhancements.Player.FasterPushAndPull",
          "Faster Push/Pull",
          { { "VB_GREAT_BAY_GEAR_CLAMP_PUSH_SPEED (Player/FasterPushAndPull.cpp)",
              W6_PLAYER_VB_COUNT(VB_GREAT_BAY_GEAR_CLAMP_PUSH_SPEED), 1 },
            { "VB_PUSH_BLOCK_SET_SPEED (Player/FasterPushAndPull.cpp)", W6_PLAYER_VB_COUNT(VB_PUSH_BLOCK_SET_SPEED),
              1 },
            { "VB_PUSH_BLOCK_SET_TIMER (Player/FasterPushAndPull.cpp)", W6_PLAYER_VB_COUNT(VB_PUSH_BLOCK_SET_TIMER),
              1 },
            { "VB_SKATE_BLOCK_BEGIN_MOVE (Player/FasterPushAndPull.cpp)", W6_PLAYER_VB_COUNT(VB_SKATE_BLOCK_BEGIN_MOVE),
              1 },
            { "VB_BLOCK_BEGIN_MOVE (Player/FasterPushAndPull.cpp)", W6_PLAYER_VB_COUNT(VB_BLOCK_BEGIN_MOVE), 1 },
            { "VB_BLOCK_BE_FINISHED_PULLING (Player/FasterPushAndPull.cpp)",
              W6_PLAYER_VB_COUNT(VB_BLOCK_BE_FINISHED_PULLING), 1 } } },
        { "gEnhancements.Player.PreventDiveOverWater",
          "Prevent Diving Over Water",
          { { "VB_LINK_DIVE_OVER_WATER (Player/PreventDiveOverWater.cpp)", W6_PLAYER_VB_COUNT(VB_LINK_DIVE_OVER_WATER),
              1 } } },
        { "gEnhancements.Player.ManualJump",
          "Manual Jump",
          { { "VB_START_JUMPSLASH (Player/ManualJump.cpp)", W6_PLAYER_VB_COUNT(VB_START_JUMPSLASH), 1 } } },
        { "gEnhancements.PlayerActions.InstantRecall",
          "Instant Fin Boomerangs Recall",
          { { "OnActorUpdate[ACTOR_EN_BOOM] (Equipment/InstantRecall.cpp)",
              []() { return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_EN_BOOM); }, 1 } } },
        { "gEnhancements.Equipment.TwoHandedSwordSpinAttack",
          "Two-Handed Sword Spin Attack",
          { { "VB_CHECK_HELD_ITEM_BUTTON_PRESS (Equipment/TwoHandedSwordSpinAttack.cpp)",
              W6_PLAYER_VB_COUNT(VB_CHECK_HELD_ITEM_BUTTON_PRESS), 1 },
            { "VB_MAGIC_SPIN_ATTACK_CHECK_FORM (Equipment/TwoHandedSwordSpinAttack.cpp)",
              W6_PLAYER_VB_COUNT(VB_MAGIC_SPIN_ATTACK_CHECK_FORM), 1 },
            { "VB_TRANSFORM_THUNDER_MATRIX (Equipment/TwoHandedSwordSpinAttack.cpp)",
              W6_PLAYER_VB_COUNT(VB_TRANSFORM_THUNDER_MATRIX), 1 } } },
        { "gEnhancements.Equipment.BetterPictoMessage",
          "Better Picto Message",
          { { "OnOpenText[0xF8] (Equipment/BetterPictoMessage.cpp)",
              []() { return SettledCountForId<GameInteractor::OnOpenText>(0xF8); }, 1 } } },
        { kChuDropsCVar,
          "MM Enable Bombchu Drops",
          { { "ShouldActorInit[ACTOR_EN_ITEM00] (Equipment/ChuDrops.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_ITEM00); }, 1 } } },
        { "gEnhancements.Equipment.GreatFairySwordBButton",
          "Great Fairy Sword B-Button Attack",
          { { "VB_GET_ITEM_ON_BUTTON (Equipment/GreatFairySwordBButton.cpp)", W6_PLAYER_VB_COUNT(VB_GET_ITEM_ON_BUTTON),
              1 } } },
    };
    return probes;
}

RegisterW6Phase sW6PlayerPhase{ "W6-PLAYER (#1236)", W6PlayerProbes };

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
        // #1239 (W6-AUDIO-SFX): the Audio Editor's four other Sfx TUs.
        { "gAudioEditor.MuteCarpenterSfx",
          "MM Mute Carpenter Sounds",
          { { "ShouldActorInit[ACTOR_OBJ_SOUND] (Audio/Sfx/MuteCarpenterSfx.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_OBJ_SOUND); }, 1 } } },
        { "gAudioEditor.ChildGoronCry",
          "MM Mute Crying Goron Child",
          { { "VB_PLAY_GORON_CHILD_CRY (Audio/Sfx/MuteCryingGoronChild.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_PLAY_GORON_CHILD_CRY); },
              1 } } },
        { "gAudioEditor.DisableNaviCallAudio",
          "Disable Navi Call Audio",
          { { "VB_PLAY_TATL_CALL_AUDIO (Audio/Sfx/DisableTatlCallAudio.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_PLAY_TATL_CALL_AUDIO); },
              1 } } },
        { "gAudioEditor.LinkVoiceFreqMultiplier",
          "Link's Voice Pitch Multiplier",
          { { "VB_LINK_VOICE_PITCH_MULTIPLIER (Audio/Sfx/LinksVoicePitchMultiplier.cpp)",
              []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_LINK_VOICE_PITCH_MULTIPLIER); },
              1 } },
          1.5f },
    };
    return probes;
}

// #1239: turn a probed key on (an integer 1, or its float) and off (0, or cleared).
void AudioProbeKeyOn(const SkipKeyProbe& probe) {
    if (probe.onFloat != 0.0f) {
        CVarSetFloat(probe.key, probe.onFloat);
    } else {
        CVarSetInteger(probe.key, 1);
    }
}
void AudioProbeKeyOff(const SkipKeyProbe& probe) {
    if (probe.onFloat != 0.0f) {
        CVarClear(probe.key);
    } else {
        CVarSetInteger(probe.key, 0);
    }
}

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetAudioKeys() {
    for (const auto& probe : AudioProbes()) {
        AudioProbeKeyOff(probe);
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

// ---- W6-CAM-GYRO (#1235, #1236, #1240): the debug camera, Remote Bombchu Control and the speed modifiers -----------
// Camera/DebugCam.cpp, Equipment/RemoteBombchu.cpp and Player/LinkSpeedModifier.cpp each register only through a
// file-scope RegisterShipInitFunc. The debug camera is driven with Free Look held on: both answer
// VB_USE_CUSTOM_CAMERA, and the debug camera must add its own registrant beside Free Look's, leaving Free Look's
// legs in place (Phase 8 drives Free Look alone). LinkSpeedModifier ANDs the master enable with the walk or swim
// one, so each of those is driven with the master held on; the toggle re-runs the registrar and must stack nothing.
// Hooks: VB_USE_CUSTOM_CAMERA (z_camera.c), VB_SPEED_MODIFIER_WALK / _SWIM (z_player.c) via
// MM_GameHooks_ExecuteVBShould; OnPassPlayerInputs (z_player.c via GameInteractor_ExecuteOnPassPlayerInputs);
// OnConsoleLogoUpdate (z_title.c); OnActorInit / OnActorUpdate / OnActorDestroy keyed on ACTOR_EN_BOM_CHU (z_actor.c
// via the MM_GameHooks_Execute* ForID legs). Each key is read by its TU alone, so the deltas are attributable.
#define CAMGYRO_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }
#define CAMGYRO_ID_COUNT(hook, id) []() { return SettledCountForId<GameInteractor::hook>(id); }
#define CAMGYRO_UNKEYED_COUNT(hook) []() { return SettledUnkeyedCount<GameInteractor::hook>(); }

const std::vector<SkipKeyProbe>& CamGyroProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gEnhancements.Camera.DebugCam.Enable",
          "MM Debug Camera (beside Free Look)",
          { { "VB_USE_CUSTOM_CAMERA (Camera/DebugCam.cpp, Free Look's own held)",
              CAMGYRO_VB_COUNT(VB_USE_CUSTOM_CAMERA), 1 },
            { "OnPassPlayerInputs, unkeyed (Camera/DebugCam.cpp)", CAMGYRO_UNKEYED_COUNT(OnPassPlayerInputs), 1 },
            { "OnCameraChangeModeFlags, unkeyed (Camera/FreeLook.cpp, untouched)",
              CAMGYRO_UNKEYED_COUNT(OnCameraChangeModeFlags), 0 } },
          0.0f,
          { "gSettings.FreeLook.Enabled" } },
        { "gEnhancements.PlayerActions.RemoteBombchu",
          "Remote Bombchu Control",
          { { "OnActorInit[EN_BOM_CHU] (Equipment/RemoteBombchu.cpp)", CAMGYRO_ID_COUNT(OnActorInit, ACTOR_EN_BOM_CHU),
              1 },
            { "OnActorUpdate[EN_BOM_CHU] (Equipment/RemoteBombchu.cpp)",
              CAMGYRO_ID_COUNT(OnActorUpdate, ACTOR_EN_BOM_CHU), 1 },
            { "OnActorDestroy[EN_BOM_CHU] (Equipment/RemoteBombchu.cpp)",
              CAMGYRO_ID_COUNT(OnActorDestroy, ACTOR_EN_BOM_CHU), 1 } } },
        { "gSettings.SpeedModifier.WalkEnable",
          "MM Enable Walk Speed Modifier (master enable held on)",
          { { "VB_SPEED_MODIFIER_WALK (Player/LinkSpeedModifier.cpp)", CAMGYRO_VB_COUNT(VB_SPEED_MODIFIER_WALK), 1 },
            { "VB_SPEED_MODIFIER_SWIM (Player/LinkSpeedModifier.cpp, swim off)",
              CAMGYRO_VB_COUNT(VB_SPEED_MODIFIER_SWIM), 0 },
            { "OnPassPlayerInputs, unkeyed (Player/LinkSpeedModifier.cpp)", CAMGYRO_UNKEYED_COUNT(OnPassPlayerInputs),
              1 },
            { "OnConsoleLogoUpdate, unkeyed (Player/LinkSpeedModifier.cpp)", CAMGYRO_UNKEYED_COUNT(OnConsoleLogoUpdate),
              1 } },
          0.0f,
          { "gSettings.SpeedModifier.Enable" } },
        { "gSettings.SpeedModifier.SwimEnable",
          "MM Enable Swim Speed Modifier (master enable held on)",
          { { "VB_SPEED_MODIFIER_SWIM (Player/LinkSpeedModifier.cpp)", CAMGYRO_VB_COUNT(VB_SPEED_MODIFIER_SWIM), 1 },
            { "VB_SPEED_MODIFIER_WALK (Player/LinkSpeedModifier.cpp, walk off)",
              CAMGYRO_VB_COUNT(VB_SPEED_MODIFIER_WALK), 0 },
            { "OnPassPlayerInputs, unkeyed (Player/LinkSpeedModifier.cpp)", CAMGYRO_UNKEYED_COUNT(OnPassPlayerInputs),
              1 } },
          0.0f,
          { "gSettings.SpeedModifier.Enable" } },
        { "gSettings.SpeedModifier.Toggle",
          "MM Toggle modifier instead of holding (master and walk held on)",
          { { "VB_SPEED_MODIFIER_WALK (Player/LinkSpeedModifier.cpp, re-run, no stacking)",
              CAMGYRO_VB_COUNT(VB_SPEED_MODIFIER_WALK), 0 },
            { "OnPassPlayerInputs, unkeyed (Player/LinkSpeedModifier.cpp, re-run, no stacking)",
              CAMGYRO_UNKEYED_COUNT(OnPassPlayerInputs), 0 } },
          0.0f,
          { "gSettings.SpeedModifier.Enable", "gSettings.SpeedModifier.WalkEnable" } },
    };
    return probes;
}

RegisterW6Phase sCamGyroPhase{ "W6-CAM-GYRO (#1235, #1236, #1240)", CamGyroProbes };

// ---- Phase 9 (#1030-#1034): the five key-gap TUs ---------------------------
// Same shape as Phase 5 (SkipKeyProbe).
#define GAP_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }
#define GAP_HOOK_COUNT(hook, id) []() { return SettledCountForId<GameInteractor::hook>(id); }

const std::vector<SkipKeyProbe>& KeyGapProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gSettings.A11yNoScreenFlashForFinishingBlow",
          "Disable Screen Flash for Finishing Blow",
          { { "VB_FLASH_SCREEN_FOR_ENEMY_KILL (Accessibility/NoScreenFlashForEnemyKill.cpp)",
              GAP_VB_COUNT(VB_FLASH_SCREEN_FOR_ENEMY_KILL), 1 } } },
        { "gEnhancements.NewDrops",
          "Enable 3D Dropped Items/Projectiles",
          { { "VB_DRAW_SLIME_BODY_ITEM (Graphics/3DItemDrops.cpp)", GAP_VB_COUNT(VB_DRAW_SLIME_BODY_ITEM), 1 },
            { "OnActorInit[EN_ITEM00] (Graphics/3DItemDrops.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_ITEM00), 1 },
            { "OnActorUpdate[EN_ITEM00] (Graphics/3DItemDrops.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_ITEM00),
              1 },
            { "OnActorInit[EN_TANRON5] (Graphics/3DItemDrops.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_TANRON5), 1 },
            { "OnActorUpdate[EN_TANRON5] (Graphics/3DItemDrops.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_TANRON5),
              1 } } },
        { "gEnhancements.InstantScarecrow",
          "Skip Playing Scarecrow's Song",
          { { "VB_NEED_SCARECROW_SONG (Songs/SkipScarecrowSong.cpp)", GAP_VB_COUNT(VB_NEED_SCARECROW_SONG), 1 } } },
        { "gEnhancements.BowArrowCycle",
          "Arrow Cycle",
          { { "VB_SHIELD_FROM_BUTTON_HOLD (Equipment/ArrowCycle.cpp)", GAP_VB_COUNT(VB_SHIELD_FROM_BUTTON_HOLD), 1 },
            { "VB_EXIT_FIRST_PERSON_MODE_FROM_BUTTON (Equipment/ArrowCycle.cpp)",
              GAP_VB_COUNT(VB_EXIT_FIRST_PERSON_MODE_FROM_BUTTON), 1 },
            { "OnActorUpdate[PLAYER] (Equipment/ArrowCycle.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_PLAYER), 1 } } },
        // #1035, #1036, #1038: the three providers PR #1046 mapped and the first link wave left out.
        { "gEnhancements.SkipArrowAnimation",
          "Skip Magic Arrow Equip Animation",
          { { "OnKaleidoUpdate, unkeyed (Equipment/SkipMagicArrowEquip.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnKaleidoUpdate>(); }, 1 } } },
        { "gEnhancements.PermanentHeartLoss",
          "Permanent Heart Loss",
          { { "OnActorUpdate[PLAYER] (DifficultyOptions/PermanentHeartLoss.cpp)",
              GAP_HOOK_COUNT(OnActorUpdate, ACTOR_PLAYER), 1 } } },
        { "gEnhancements.N64WeirdFrames",
          "N64 Weird Frames",
          { { "VB_LOAD_PLAYER_ANIMATION_FRAME (Restorations/N64WeirdFrames/N64WeirdFrames.cpp)",
              GAP_VB_COUNT(VB_LOAD_PLAYER_ANIMATION_FRAME), 1 } } },
        // #1037: the held-apart damage multiplier (MM's own key, hosted beside SoH's row).
        { "gEnhancements.DifficultyOptions.DamageMultiplier",
          "Damage Multiplier",
          { { "VB_MULTIPLY_INFLICTED_DMG (DifficultyOptions/DamageMultiplier.cpp)",
              GAP_VB_COUNT(VB_MULTIPLY_INFLICTED_DMG), 1 } } },
        // Skip Misc Interactions: 25 TUs under Cutscenes/MiscInteractions/, one key. Per registry, the contributors:
        //   VB_START_CUTSCENE 8 = AlienStartAndFail, DekuTelescope, GivingBombersNotebook, KamaroTeachDance,
        //     KotakeFlying, MayorsOfficeCutscene, RosaSistersDance, TurtleGoodbye.
        //   VB_PLAY_TRANSITION_CS 3 = AlienStartAndFail, EvanSong, TatlInterrupts.
        //   ShouldActorInit: EN_MA4 (AlienWarning), DOOR_WARP1 (BossWarpPrompt), EN_SELLNUTS (DekuSalesman),
        //     EN_GEG (HungryGoronDialogue), EN_ZOG (PushingMikau), EN_BAL (TingleTime).
        //   OnActorInit: EN_GINKO_MAN (BankerDialogue), EN_STOP_HEISHI (GateGuards), EN_BOM_BOWL_MAN
        //     (GivingBombersNotebook), EN_OWL (OwlInteractions).
        //   OnActorUpdate: EN_SELLNUTS and EN_AKINDONUTS (DekuSalesman), EN_SCOPENUTS (DekuTelescope),
        //     EN_BJI_01 (FallingMoonsTear), EN_KAKASI (ScarecrowDance). OnActorDestroy: EN_BJI_01 (FallingMoonsTear).
        //   OnOpenText: 0x72F and 0x716 (GivingBombersNotebook), 0x296A (KafeiReveal).
        //   The rest are single VB buckets, one TU each (named on the row).
        { "gEnhancements.TimeSavers.SkipMiscInteractions",
          "Skip Misc Interactions",
          { { "VB_START_CUTSCENE", GAP_VB_COUNT(VB_START_CUTSCENE), 8 },
            { "VB_PLAY_TRANSITION_CS", GAP_VB_COUNT(VB_PLAY_TRANSITION_CS), 3 },
            { "VB_QUEUE_CUTSCENE (SkipEponaReveal.cpp)", GAP_VB_COUNT(VB_QUEUE_CUTSCENE), 1 },
            { "VB_CONTINUE_BANKER_DIALOGUE (SkipBankerDialogue.cpp)", GAP_VB_COUNT(VB_CONTINUE_BANKER_DIALOGUE), 1 },
            { "VB_MONKEY_WAIT_TO_TALK_AFTER_APPROACH (SkipBothersomeMonkey.cpp)",
              GAP_VB_COUNT(VB_MONKEY_WAIT_TO_TALK_AFTER_APPROACH), 1 },
            { "VB_SETUP_EAST_CLOCK_TOWN_BOM_BOWL_MAN (SkipGivingBombersNotebook.cpp)",
              GAP_VB_COUNT(VB_SETUP_EAST_CLOCK_TOWN_BOM_BOWL_MAN), 1 },
            { "VB_START_GREAT_FAIRY_CUTSCENE (SkipGreatFairyCutscene.cpp)", GAP_VB_COUNT(VB_START_GREAT_FAIRY_CUTSCENE),
              1 },
            { "VB_OWL_TELL_ABOUT_SHRINE (SkipOwlInteractions.cpp)", GAP_VB_COUNT(VB_OWL_TELL_ABOUT_SHRINE), 1 },
            { "VB_TATL_INTERRUPT_MSG (SkipTatlInterrupts.cpp)", GAP_VB_COUNT(VB_TATL_INTERRUPT_MSG), 1 },
            { "VB_TATL_INTERRUPT_MSG3 (SkipTatlInterrupts.cpp)", GAP_VB_COUNT(VB_TATL_INTERRUPT_MSG3), 1 },
            { "VB_TATL_INTERRUPT_MSG4 (SkipTatlInterrupts.cpp)", GAP_VB_COUNT(VB_TATL_INTERRUPT_MSG4), 1 },
            { "VB_TATL_INTERRUPT_MSG6 (SkipTatlInterrupts.cpp)", GAP_VB_COUNT(VB_TATL_INTERRUPT_MSG6), 1 },
            { "ShouldActorInit[EN_MA4] (SkipAlienWarning.cpp)", GAP_HOOK_COUNT(ShouldActorInit, ACTOR_EN_MA4), 1 },
            { "ShouldActorInit[DOOR_WARP1] (SkipBossWarpPrompt.cpp)", GAP_HOOK_COUNT(ShouldActorInit, ACTOR_DOOR_WARP1),
              1 },
            { "ShouldActorInit[EN_SELLNUTS] (SkipDekuSalesman.cpp)", GAP_HOOK_COUNT(ShouldActorInit, ACTOR_EN_SELLNUTS),
              1 },
            { "ShouldActorInit[EN_GEG] (SkipHungryGoronDialogue.cpp)", GAP_HOOK_COUNT(ShouldActorInit, ACTOR_EN_GEG),
              1 },
            { "ShouldActorInit[EN_ZOG] (SkipPushingMikau.cpp)", GAP_HOOK_COUNT(ShouldActorInit, ACTOR_EN_ZOG), 1 },
            { "ShouldActorInit[EN_BAL] (SkipTingleTime.cpp)", GAP_HOOK_COUNT(ShouldActorInit, ACTOR_EN_BAL), 1 },
            { "OnActorInit[EN_GINKO_MAN] (SkipBankerDialogue.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_GINKO_MAN),
              1 },
            { "OnActorInit[EN_STOP_HEISHI] (SkipGateGuards.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_STOP_HEISHI),
              1 },
            { "OnActorInit[EN_BOM_BOWL_MAN] (SkipGivingBombersNotebook.cpp)",
              GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_BOM_BOWL_MAN), 1 },
            { "OnActorInit[EN_OWL] (SkipOwlInteractions.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_OWL), 1 },
            { "OnActorUpdate[EN_SELLNUTS] (SkipDekuSalesman.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_SELLNUTS),
              1 },
            { "OnActorUpdate[EN_AKINDONUTS] (SkipDekuSalesman.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_AKINDONUTS),
              1 },
            { "OnActorUpdate[EN_SCOPENUTS] (SkipDekuTelescope.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_SCOPENUTS),
              1 },
            { "OnActorUpdate[EN_BJI_01] (SkipFallingMoonsTear.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_BJI_01),
              1 },
            { "OnActorUpdate[EN_KAKASI] (SkipScarecrowDance.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_KAKASI), 1 },
            { "OnActorDestroy[EN_BJI_01] (SkipFallingMoonsTear.cpp)", GAP_HOOK_COUNT(OnActorDestroy, ACTOR_EN_BJI_01),
              1 },
            { "OnOpenText[0x72F] (SkipGivingBombersNotebook.cpp)", GAP_HOOK_COUNT(OnOpenText, 0x72F), 1 },
            { "OnOpenText[0x716] (SkipGivingBombersNotebook.cpp)", GAP_HOOK_COUNT(OnOpenText, 0x716), 1 },
            { "OnOpenText[0x296A] (SkipKafeiReveal.cpp)", GAP_HOOK_COUNT(OnOpenText, 0x296A), 1 } } },
        // #1123: Skip Boss Introductions. One TU, Cutscenes/SkipEnemyCutscenes.cpp, three registrars on the one key:
        //   VB_START_CUTSCENE 2 = the boss-warp leg and the enemy-cutscene leg. The other buckets are one each; a
        //   representative set of its ShouldActorInit / OnActorInit / ShouldActorUpdate / OnActorUpdate ids is probed.
        { "gEnhancements.TimeSavers.SkipCutscene.BossIntro",
          "Skip Boss Introductions",
          { { "VB_START_CUTSCENE (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_VB_COUNT(VB_START_CUTSCENE), 2 },
            { "VB_SET_CAMERA_AT_EYE (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_VB_COUNT(VB_SET_CAMERA_AT_EYE), 1 },
            { "VB_SET_CAMERA_FOV (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_VB_COUNT(VB_SET_CAMERA_FOV), 1 },
            { "VB_PLAYER_CUTSCENE_ACTION (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_VB_COUNT(VB_PLAYER_CUTSCENE_ACTION),
              1 },
            { "ShouldActorInit[BOSS_01] (Cutscenes/SkipEnemyCutscenes.cpp)",
              GAP_HOOK_COUNT(ShouldActorInit, ACTOR_BOSS_01), 1 },
            { "ShouldActorInit[BOSS_07] (Cutscenes/SkipEnemyCutscenes.cpp)",
              GAP_HOOK_COUNT(ShouldActorInit, ACTOR_BOSS_07), 1 },
            { "ShouldActorInit[EN_DINOFOS] (Cutscenes/SkipEnemyCutscenes.cpp)",
              GAP_HOOK_COUNT(ShouldActorInit, ACTOR_EN_DINOFOS), 1 },
            { "OnActorInit[EN_BIGPO] (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_BIGPO),
              1 },
            { "OnActorInit[EN_WIZ] (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_HOOK_COUNT(OnActorInit, ACTOR_EN_WIZ), 1 },
            { "ShouldActorUpdate[EN_KNIGHT] (Cutscenes/SkipEnemyCutscenes.cpp)",
              GAP_HOOK_COUNT(ShouldActorUpdate, ACTOR_EN_KNIGHT), 1 },
            { "OnActorUpdate[BOSS_06] (Cutscenes/SkipEnemyCutscenes.cpp)", GAP_HOOK_COUNT(OnActorUpdate, ACTOR_BOSS_06),
              1 },
            { "OnActorUpdate[EN_KAIZOKU] (Cutscenes/SkipEnemyCutscenes.cpp)",
              GAP_HOOK_COUNT(OnActorUpdate, ACTOR_EN_KAIZOKU), 1 } } },
    };
    return probes;
}

// ---- W6-MODES-DIFF (#1236, #1240): MM's Modes, Difficulty and 'Other' TUs, and the Mirrored World translator ------
// One probe per key; each TU registers only through a file-scope RegisterShipInitFunc. Attributed by registry content:
// every bucket is keyed on a VB id, an actor id, a limb or a text id, or is an unkeyed registry no other W6 key arms
// while this key is the only one on. Mirrored World's key is SoH's gEnhancements.MirroredWorldMode (1 = Always): the
// translator (Modes/MirroredWorld.cpp) arms one unkeyed OnSceneInit hook for any mode but Off. DeleteFileOnDeath.cpp
// is not linked (INTENTIONAL: MM's SaveManager is not in the single exe, and a paired file may only be deleted whole),
// so it has no probe.
#define W6_MODES_VB_COUNT(vb) []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(vb); }
#define W6_MODES_ID_COUNT(hook, id) []() { return SettledCountForId<GameInteractor::hook>(id); }

constexpr const char* kW6JinxedTimerCVar = "gEnhancements.DifficultyOptions.JinxedTimer";

const std::vector<SkipKeyProbe>& W6ModesDifficultyProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { "gModes.PlayAsKafei",
          "Play as Kafei",
          { { "OnPlayDestroy, unkeyed (Modes/PlayAsKafei.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnPlayDestroy>(); }, 1 } } },
        { "gModes.HyruleWarriorsStyledLink",
          "Hyrule Warriors Styled Link",
          { { "OnPlayerPostLimbDraw[PLAYER_LIMB_HEAD] (Modes/HyruleWarriorsStyledLink.cpp)",
              W6_MODES_ID_COUNT(OnPlayerPostLimbDraw, PLAYER_LIMB_HEAD), 1 },
            { "OnPlayerPostLimbDraw[PLAYER_LIMB_WAIST] (Modes/HyruleWarriorsStyledLink.cpp)",
              W6_MODES_ID_COUNT(OnPlayerPostLimbDraw, PLAYER_LIMB_WAIST), 1 } } },
        { "gModes.TimeMovesWhenYouMove",
          "Time Moves when you Move",
          { { "OnActorUpdate[ACTOR_PLAYER] (Modes/TimeMovesWhenYouMove.cpp)",
              W6_MODES_ID_COUNT(OnActorUpdate, ACTOR_PLAYER), 1 } } },
        { "gEnhancements.MirroredWorldMode",
          "Mirrored World (SoH's row, translated)",
          { { "OnSceneInit, unkeyed (Modes/MirroredWorld.cpp)",
              []() { return SettledUnkeyedCount<GameInteractor::OnSceneInit>(); }, 1 } } },
        // A slider that arms BELOW its default (`CVAR < 60`), so the drive's 0 and 1 both arm it. This count, read
        // while the key is 0 (before the drive turns it on, and after it turns it off), first re-drives it at 60
        // (the default, unarmed), so the drive's 1 measures exactly the TU's one registration. Without the TU in the
        // link nothing arms and the probe is red. It leaves the key at its default, 60.
        { kW6JinxedTimerCVar,
          "Jinxed Timer",
          { { "ShouldActorUpdate[ACTOR_PLAYER] (DifficultyOptions/JinxedTimer.cpp)",
              []() {
                  if (CVarGetInteger(kW6JinxedTimerCVar, 60) == 0) {
                      CVarSetInteger(kW6JinxedTimerCVar, 60);
                      ShipInit::Init(kW6JinxedTimerCVar);
                  }
                  return SettledCountForId<GameInteractor::ShouldActorUpdate>(ACTOR_PLAYER);
              },
              1 } } },
        { "gEnhancements.DifficultyOptions.LowerBankRewardThresholds",
          "Lower Bank Reward Thresholds",
          { { "VB_PASS_FIRST_BANK_THRESHOLD (DifficultyOptions/CustomBankRewardThresholds.cpp)",
              W6_MODES_VB_COUNT(VB_PASS_FIRST_BANK_THRESHOLD), 1 },
            { "VB_PASS_SECOND_BANK_THRESHOLD_ALT (DifficultyOptions/CustomBankRewardThresholds.cpp)",
              W6_MODES_VB_COUNT(VB_PASS_SECOND_BANK_THRESHOLD_ALT), 1 },
            { "OnOpenText[0x045B] (DifficultyOptions/CustomBankRewardThresholds.cpp)",
              W6_MODES_ID_COUNT(OnOpenText, 0x045B), 1 } } },
        { "gEnhancements.DifficultyOptions.DisableTakkuriSteal",
          "Disable Takkuri Steal",
          { { "VB_THIEF_BIRD_STEAL (DifficultyOptions/DisableTakkuriSteal.cpp)", W6_MODES_VB_COUNT(VB_THIEF_BIRD_STEAL),
              1 } } },
        { "gEnhancements.DifficultyOptions.DekuGuardSearchBalls",
          "Deku Guard Search Balls",
          { { "VB_DEKU_GUARD_SHOW_SEARCH_BALLS (DifficultyOptions/DekuGuardSearchBalls.cpp)",
              W6_MODES_VB_COUNT(VB_DEKU_GUARD_SHOW_SEARCH_BALLS), 1 } } },
        { "gEnhancements.DifficultyOptions.GibdoTradeSequence",
          "Gibdo Trade Sequence Options",
          { { "VB_GIBDO_TRADE_SEQUENCE_SUFFICIENT_QUANTITY_PRESENTED (DifficultyOptions/GibdoTradeSequenceOptions.cpp)",
              W6_MODES_VB_COUNT(VB_GIBDO_TRADE_SEQUENCE_SUFFICIENT_QUANTITY_PRESENTED), 1 },
            { "VB_GIBDO_TRADE_SEQUENCE_ACCEPT_RED_POTION (DifficultyOptions/GibdoTradeSequenceOptions.cpp)",
              W6_MODES_VB_COUNT(VB_GIBDO_TRADE_SEQUENCE_ACCEPT_RED_POTION), 1 },
            { "VB_GIBDO_TRADE_SEQUENCE_TAKE_MORE_THAN_ONE_ITEM (DifficultyOptions/GibdoTradeSequenceOptions.cpp)",
              W6_MODES_VB_COUNT(VB_GIBDO_TRADE_SEQUENCE_TAKE_MORE_THAN_ONE_ITEM), 1 } } },
        { "gEnhancements.DifficultyOptions.HiddenGrottosVisibility",
          "Hidden Grottos Visibility",
          { { "OnActorDraw[ACTOR_DOOR_ANA] (DifficultyOptions/HiddenGrottosVisibility.cpp)",
              W6_MODES_ID_COUNT(OnActorDraw, ACTOR_DOOR_ANA), 1 } } },
        { "gEnhancements.DifficultyOptions.FrogChoirCount",
          "Frog Choir Count",
          { { "OnActorUpdate[ACTOR_EN_MINIFROG] (Minigames/FrogChoirCount.cpp)",
              W6_MODES_ID_COUNT(OnActorUpdate, ACTOR_EN_MINIFROG), 1 } } },
        { "gEnhancements.Minigames.CremiaHugs",
          "Milk Run Reward Options",
          { { "VB_PLAY_CREMIA_HUG_CUTSCENE (Minigames/CremiaHugs.cpp)", W6_MODES_VB_COUNT(VB_PLAY_CREMIA_HUG_CUTSCENE),
              1 } } },
        { "gEnhancements.Items.AmmoBuyback",
          "Ammo Buyback Options",
          { { "OnActorUpdate[ACTOR_EN_FSN] (Items/AmmoBuyback.cpp)", W6_MODES_ID_COUNT(OnActorUpdate, ACTOR_EN_FSN),
              1 },
            { "OnActorDestroy[ACTOR_EN_FSN] (Items/AmmoBuyback.cpp)", W6_MODES_ID_COUNT(OnActorDestroy, ACTOR_EN_FSN),
              1 },
            { "OnOpenText[0x29EF] (Items/AmmoBuyback.cpp)", W6_MODES_ID_COUNT(OnOpenText, 0x29EF), 1 },
            { "VB_MSG_LOAD_RUPEES_TEXT (Items/AmmoBuyback.cpp)", W6_MODES_VB_COUNT(VB_MSG_LOAD_RUPEES_TEXT), 1 } } },
    };
    return probes;
}

RegisterW6Phase sW6ModesDifficultyPhase{ "W6-MODES-DIFF (#1236, #1240)", W6ModesDifficultyProbes };

// Puts every probed key back to 0 and re-drives it, so the registrants a probe
// armed are queued for removal and the row leaves no hook behind.
void ResetKeyGapKeys() {
    for (const auto& probe : KeyGapProbes()) {
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
    }
    for (const auto& probe : KeyGapProbes()) {
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
}

// ---- W6 phases (#1233): the zero and run helpers ---------------------------
// The list itself is above Phase 4 (W6Phases). Both helpers take the list, so the control below can drive a synthetic
// phase through the very code the registered ones go through.

// W6-CAM-GYRO: sets each of @p probe's held keys (SkipKeyProbe::alsoOn) to @p value and drives it.
void DriveW6HeldKeys(const SkipKeyProbe& probe, int32_t value) {
    for (const char* key : probe.alsoOn) {
        CVarSetInteger(key, value);
        ShipInit::Init(key);
    }
}

void ZeroW6PhaseKeys(const std::vector<W6Phase>& phases) {
    for (const W6Phase& phase : phases) {
        for (const auto& probe : phase.probes()) {
            CVarSetInteger(probe.key, 0);
            for (const char* key : probe.alsoOn) {
                CVarSetInteger(key, 0);
            }
        }
    }
}

void ResetW6PhaseKeys(const std::vector<W6Phase>& phases) {
    for (const W6Phase& phase : phases) {
        for (const auto& probe : phase.probes()) {
            CVarSetInteger(probe.key, 0);
            ShipInit::Init(probe.key);
            DriveW6HeldKeys(probe, 0);
        }
    }
    for (const W6Phase& phase : phases) {
        for (const auto& probe : phase.probes()) {
            for (const auto& reg : probe.registries) {
                (void)reg.count();
            }
        }
    }
}

/** Drive every probe of every phase as Phases 5-9 do. Returns 0, or 18 naming the phase and key (printed only when
 *  @p report: the control expects its 18 and must not print a FAIL line). Leaves every key off and re-driven. */
int RunW6Phases(const std::vector<W6Phase>& phases, bool report) {
    for (const W6Phase& phase : phases) {
        for (const auto& probe : phase.probes()) {
            DriveW6HeldKeys(probe, 1);
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
                    if (report) {
                        printf("[TEST] FAIL(18): %s: %s went %zu -> %zu when %s was turned on and driven twice, "
                               "expected exactly +%zu - the TU(s) behind the \"%s\" row are not in the link (a plain "
                               "2ship_enh archive drops a TU whose only anchor is a RegisterShipInitFunc), or one "
                               "stacks a registration per toggle, so the row SoH's menu draws does nothing or "
                               "misbehaves in Majora's Mask (#1233)\n",
                               phase.name, reg.what, before[i], after, probe.key, reg.expected, probe.row);
                    }
                    ResetW6PhaseKeys(phases);
                    return 18;
                }
            }
            CVarSetInteger(probe.key, 0);
            ShipInit::Init(probe.key);
            DriveW6HeldKeys(probe, 0);
            for (const auto& reg : probe.registries) {
                (void)reg.count();
            }
        }
    }
    ResetW6PhaseKeys(phases);
    return 0;
}

// The control's synthetic phase: a key no registrar is keyed on, claimed to arm one OnGameStateUpdate hook. The run
// helper must refuse it with 18, or its green over the registered phases proves nothing.
constexpr const char* kW6ControlKey = "gEnhancements.RsbsW6PhaseControl";

const std::vector<SkipKeyProbe>& W6ControlProbes() {
    static const std::vector<SkipKeyProbe> probes = {
        { kW6ControlKey,
          "Synthetic W6 Control",
          { { "OnGameStateUpdate (no TU: the control)",
              []() { return SettledUnkeyedCount<GameInteractor::OnGameStateUpdate>(); }, 1 } } },
    };
    return probes;
}

/** The runner's one W6 call: the control first (a synthetic phase whose key arms nothing must come back 18, quietly),
 *  then every registered phase. Returns 0 or 18. */
int RunW6PhaseList() {
    const std::vector<W6Phase> control = { W6Phase{ "W6 control (synthetic)", W6ControlProbes } };
    const int controlRc = RunW6Phases(control, false);
    CVarClear(kW6ControlKey);
    if (controlRc != 18) {
        printf("[TEST] FAIL(18): the W6 phase drive passed a synthetic phase whose key arms nothing (rc %d), so its "
               "pass over the registered phases would prove nothing (#1233)\n",
               controlRc);
        return 18;
    }
    const int rc = RunW6Phases(W6Phases(), true);
    if (rc == 0) {
        printf("[TEST] W6 phases: the control was refused with 18; %zu registered phase(s) armed exactly their TUs\n",
               W6Phases().size());
    }
    return rc;
}

// ---- W6-PLAYER (#1236, review H1): MM Enable Bombchu Drops' conversion rule ----------
// See W6-PLAYER'S BOMBCHU DROPS RULE in the file header. Returns 0 or 22; leaves the key off, its registrant gone, the
// Bombchu slot and MM_gPlayState as found, and Ship_Random reseeded from the device as an unseeded process would be.
int RunChuDropsRuleLeg() {
    using Init = GameInteractor::ShouldActorInit;

    // A seed whose first roll keeps the Bomb and whose second differs. Searched rather than hard-coded, so a change to
    // Ship_Random's generator cannot leave the leg on a first roll that converts (that would spawn into the stand-in).
    uint64_t seed = 0;
    s32 firstRoll = 0;
    s32 secondRoll = 0;
    for (uint64_t candidate = 1; candidate < 4096 && seed == 0; ++candidate) {
        Ship_Random_Seed(candidate);
        const s32 first = Ship_Random(0, 100);
        const s32 second = Ship_Random(0, 100);
        if (first >= 50 && first != second) {
            seed = candidate;
            firstRoll = first;
            secondRoll = second;
        }
    }
    RC_ASSERT(seed != 0, 22, "no Ship_Random seed below 4096 gives a first roll >= 50 and a different second roll");

    // Arm the key and take the registrant arming added. RunW6PhaseList left the key off and re-driven.
    auto registrants = []() {
        S2H::GameHooks::FlushPendingUnregistrations<Init>();
        std::vector<uint32_t> ids;
        auto& buckets = S2H::GameHooks::Registry<Init>::functionsForID;
        if (auto it = buckets.find(ACTOR_EN_ITEM00); it != buckets.end()) {
            for (auto& [id, fn] : it->second) {
                ids.push_back(id);
            }
        }
        return ids;
    };
    const std::vector<uint32_t> before = registrants();
    CVarSetInteger(kChuDropsCVar, 1);
    ShipInit::Init(kChuDropsCVar);
    ShipInit::Init(kChuDropsCVar);
    std::vector<uint32_t> added;
    for (uint32_t id : registrants()) {
        bool seen = false;
        for (uint32_t b : before) {
            seen = seen || b == id;
        }
        if (!seen) {
            added.push_back(id);
        }
    }
    auto disarm = [&]() {
        CVarSetInteger(kChuDropsCVar, 0);
        ShipInit::Init(kChuDropsCVar);
        return registrants().size() == before.size();
    };
    if (added.size() != 1) {
        printf("[TEST] FAIL(22): arming \"%s\" added %zu ShouldActorInit[EN_ITEM00] registrants, expected exactly 1 "
               "(ChuDrops.cpp's)\n",
               kChuDropsCVar, added.size());
        (void)disarm();
        return 22;
    }
    const Init::fn chuDrops = S2H::GameHooks::Registry<Init>::functionsForID[ACTOR_EN_ITEM00][added[0]];

    // A stand-in Bomb drop over a zeroed stand-in play state.
    Actor bomb;
    memset(&bomb, 0, sizeof(bomb));
    bomb.id = ACTOR_EN_ITEM00;
    bomb.params = ITEM00_BOMBS_A;
    std::vector<char> playStorage(sizeof(PlayState), 0);
    PlayState* const savedPlay = MM_gPlayState;
    MM_gPlayState = reinterpret_cast<PlayState*>(playStorage.data());
    const u8 savedChuSlot = INV_CONTENT(ITEM_BOMBCHU);

    struct Case {
        const char* held;
        u8 slot;
        int expectedRolls; // 0: the next roll drawn is firstRoll; 1: it is secondRoll
    };
    const Case cases[] = { { "no Bombchus held", ITEM_NONE, 0 }, { "Bombchus held", ITEM_BOMBCHU, 1 } };
    int failures = 0;
    for (const Case& c : cases) {
        INV_CONTENT(ITEM_BOMBCHU) = c.slot;
        Ship_Random_Seed(seed);
        bool should = true;
        chuDrops(&bomb, &should);
        const s32 next = Ship_Random(0, 100);
        const int rolls = next == firstRoll ? 0 : (next == secondRoll ? 1 : -1);
        printf("[TEST] mm-registrar-coverage: (22) %s: a Bomb drop took %d conversion roll(s) and was %s (expected "
               "%d, kept)\n",
               c.held, rolls, should ? "kept" : "converted", c.expectedRolls);
        if (rolls != c.expectedRolls || !should) {
            printf("[TEST] FAIL(22): with \"%s\" on and %s, a Bomb drop took %d conversion roll(s) (expected %d) and "
                   "was %s (expected kept: the seeded roll keeps the Bomb). MM's Enable Bombchu Drops may convert a "
                   "Bomb drop only while Link already has Bombchus: the Bombchu count is a shared PROGRESSION "
                   "quantity, and a converted drop's Item_Give hands Termina the Bombchu item and Hyrule, through "
                   "the pool, the count and the item before the seed placed them (#1236, review H1)\n",
                   kChuDropsCVar, c.held, rolls, c.expectedRolls, should ? "kept" : "converted");
            failures++;
        }
    }

    INV_CONTENT(ITEM_BOMBCHU) = savedChuSlot;
    MM_gPlayState = savedPlay;
    Ship_Random_Seed(static_cast<uint64_t>(std::random_device{}()));
    const bool disarmed = disarm();
    RC_ASSERT(disarmed, 22, "ChuDrops' registrant stayed registered after its key went back off");
    if (failures != 0) {
        return 22;
    }
    printf("[TEST] W6-PLAYER: MM Enable Bombchu Drops gives a Bomb drop its conversion roll only while Link has "
           "Bombchus\n");
    return 0;
}

// ---- Phase 12 (#1215, #1221): the rando-forced gates the link dropped ------
// See PHASE 12 in the file header for the attribution of each bucket.
struct RandoGateCounts {
    size_t durability;
    size_t offer;
    size_t flagSet;
};

RandoGateCounts ReadRandoGateCounts() {
    return { SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_LOWER_RAZOR_SWORD_DURABILITY),
             SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_GIVE_ITEM_FROM_OFFER),
             SettledUnkeyedCount<GameInteractor::OnFlagSet>() };
}

// The production rando arm path (Rando/Rando.cpp OnSaveLoadHandler's ShipInit half).
void DriveRandoArm(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    ShipInit::Init("IS_RANDO");
}

// Raw flag writes for the setup, so arranging a leg dispatches no OnFlagSet.
void RawClearWeekEventReg(u32 flag) {
    WEEKEVENTREG(flag >> 8) = GET_WEEKEVENTREG(flag >> 8) & (u8) ~(flag & 0xFF);
}

struct RandoGateVerdicts {
    bool durabilityLowers;  // VB_LOWER_RAZOR_SWORD_DURABILITY's verdict
    bool walletFlagSet;     // the Oceanside wallet check's flag after a day-2 offer
    s32 offered;            // the item that offer ends up with
    bool movedInStays;      // BUYER_MOVED_IN after Mikau's grave sets it, reward not collected
    bool movedInWithReward; // BUYER_MOVED_IN after the reward is collected
};

RandoGateVerdicts ProbeRandoGates() {
    RandoGateVerdicts v{};
    v.durabilityLowers = GameInteractor_Should(VB_LOWER_RAZOR_SWORD_DURABILITY, true);

    // The Oceanside Spider House man at the entrance, offering the day-2 reward
    // with the wallet tier at 0 and the check's flag clear.
    RawClearWeekEventReg(WEEKEVENTREG_RECEIVED_OCEANSIDE_WALLET_UPGRADE);
    Actor sth;
    memset(&sth, 0, sizeof(sth));
    sth.id = ACTOR_EN_STH;
    sth.params = STH_TYPE_OCEANSIDE_SPIDER_HOUSE_GREET;
    GetItemId gi = GI_RUPEE_PURPLE;
    (void)GameInteractor_Should(VB_GIVE_ITEM_FROM_OFFER, true, &gi, &sth);
    v.walletFlagSet = CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_OCEANSIDE_WALLET_UPGRADE);
    v.offered = (s32)gi;

    // Mikau's grave sets BUYER_MOVED_IN before the man has been spoken to.
    RawClearWeekEventReg(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
    RawClearWeekEventReg(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD);
    SET_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
    v.movedInStays = CHECK_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);

    // The reward collected: the squatter may move in now.
    RawClearWeekEventReg(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
    RawClearWeekEventReg(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD);
    SET_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD);
    v.movedInWithReward = CHECK_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
    return v;
}

// W6-CYCLE-CHEATS: the row-on leg (see the call in RunRandoGateLegs). @p disarmed is the vanilla baseline.
int RunGateRowLegs(const RandoGateCounts& disarmed) {
    struct GateRow {
        const char* key;
        size_t (*count)();
        bool (*answered)(const RandoGateVerdicts&); // null: no verdict probe (the Razor restore, MMCycleRazorSword's)
        size_t baseline;
    };
    const GateRow rows[] = {
        { "gCheats.UnbreakableRazorSword",
          []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_LOWER_RAZOR_SWORD_DURABILITY); },
          [](const RandoGateVerdicts& v) { return !v.durabilityLowers; }, disarmed.durability },
        { "gEnhancements.Cycle.OceansideWalletAnyDay",
          []() { return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_GIVE_ITEM_FROM_OFFER); },
          [](const RandoGateVerdicts& v) { return v.walletFlagSet && v.offered == GI_WALLET_ADULT; }, disarmed.offer },
        { "gEnhancements.Cycle.StopOceansideSpiderHouseSquatter",
          []() { return SettledUnkeyedCount<GameInteractor::OnFlagSet>(); },
          [](const RandoGateVerdicts& v) { return !v.movedInStays && v.movedInWithReward; }, disarmed.flagSet },
        { "gEnhancements.Cycle.DoNotResetRazorSword",
          []() { return SettledUnkeyedCount<GameInteractor::AfterEndOfCycleSave>(); }, nullptr,
          SettledUnkeyedCount<GameInteractor::AfterEndOfCycleSave>() },
    };
    for (const GateRow& row : rows) {
        CVarSetInteger(row.key, 1);
        ShipInit::Init(row.key);
        const size_t on = row.count();
        const bool answered = row.answered == nullptr || row.answered(ProbeRandoGates());
        CVarSetInteger(row.key, 0);
        ShipInit::Init(row.key);
        const size_t off = row.count();
        CVarClear(row.key);
        printf("[TEST] mm-registrar-coverage: (12) row on, rando off: %s registrants %zu -> %zu, answered %d; "
               "row off: %zu (expected +1, 1, back to %zu)\n",
               row.key, row.baseline, on, answered ? 1 : 0, off, row.baseline);
        RC_ASSERT(on == row.baseline + 1, 21,
                  "a gate's own row, turned on outside rando, did not register its gate exactly once - its TU is not "
                  "in the link, or the gate answers \"IS_RANDO\" alone, so the MM row SoH's menu draws does nothing "
                  "(#1236, #1240)");
        RC_ASSERT(answered, 21,
                  "a gate's own row, turned on outside rando, registered but did not answer as it does under rando "
                  "(#1236, #1240)");
        RC_ASSERT(off == row.baseline, 21,
                  "a gate stayed registered with its row off and rando off - nothing may arm it then (#1236, #1240)");
    }
    return 0;
}

int RunRandoGateLegs() {
    // ---- disarmed: the vanilla control and the baseline ----------------------
    DriveRandoArm(false);
    const RandoGateCounts before = ReadRandoGateCounts();
    const RandoGateVerdicts vanilla = ProbeRandoGates();
    printf("[TEST] mm-registrar-coverage: (12) vanilla file: durability lowers %d, wallet flag %d (offered 0x%02X), "
           "moved-in stays %d, moved-in with reward %d (expected 1, 0 (0x%02X), 1, 0)\n",
           vanilla.durabilityLowers, vanilla.walletFlagSet, (unsigned)vanilla.offered, vanilla.movedInStays,
           vanilla.movedInWithReward, (unsigned)GI_RUPEE_PURPLE);
    RC_ASSERT(vanilla.durabilityLowers, 18,
              "a vanilla file's Razor Sword does not lose durability - a rando-only gate leaked outside rando, "
              "and the leg-2 durability probe would be vacuous (#1215)");
    RC_ASSERT(!vanilla.walletFlagSet && vanilla.offered == GI_RUPEE_PURPLE, 18,
              "a vanilla file's day-2 Oceanside reward was rewritten - a rando-only gate leaked outside rando, and "
              "the leg-2 wallet probe would be vacuous (#1221)");
    RC_ASSERT(vanilla.movedInStays && !vanilla.movedInWithReward, 18,
              "a vanilla file's spider-house squatter flag was rewritten - a rando-only gate leaked outside rando, "
              "and the leg-2 squatter probe would be vacuous (#1221)");

    // ---- W6-CYCLE-CHEATS: each gate's own row, rando off --------------------------
    // With IS_RANDO off and the key on, the gate registers exactly once and answers as under rando; with both off it
    // is gone again. Red while the four TUs are unlinked (the port armed on "IS_RANDO" alone).
    if (const int rc = RunGateRowLegs(before); rc != 0) {
        return rc;
    }

    // ---- armed through the production path -----------------------------------
    DriveRandoArm(true);
    const RandoGateCounts after = ReadRandoGateCounts();
    printf("[TEST] mm-registrar-coverage: (12) rando arm: VB_LOWER_RAZOR_SWORD_DURABILITY %zu -> %zu, "
           "VB_GIVE_ITEM_FROM_OFFER %zu -> %zu, OnFlagSet %zu -> %zu (expected +1 each)\n",
           before.durability, after.durability, before.offer, after.offer, before.flagSet, after.flagSet);
    RC_ASSERT(after.durability == before.durability + 1, 19,
              "VB_LOWER_RAZOR_SWORD_DURABILITY did not gain exactly one registrant when rando armed - nothing "
              "answers the Razor's durability in a rando file, so ~100 hits break the seed's Razor into the Kokiri "
              "Sword (#1215)");
    RC_ASSERT(after.offer == before.offer + 1, 19,
              "VB_GIVE_ITEM_FROM_OFFER did not gain exactly one registrant when rando armed - the Oceanside Spider "
              "House reward is lost on day 2 or 3 in a rando file (#1221)");
    RC_ASSERT(after.flagSet == before.flagSet + 1, 19,
              "OnFlagSet did not gain exactly one registrant when rando armed - Mikau's grave moves the spider-house "
              "squatter in and the Oceanside reward is lost for the cycle in a rando file (#1221)");

    const RandoGateVerdicts rando = ProbeRandoGates();
    printf("[TEST] mm-registrar-coverage: (12) rando file: durability lowers %d, wallet flag %d (offered 0x%02X), "
           "moved-in stays %d, moved-in with reward %d (expected 0, 1 (0x%02X), 0, 1)\n",
           rando.durabilityLowers, rando.walletFlagSet, (unsigned)rando.offered, rando.movedInStays,
           rando.movedInWithReward, (unsigned)GI_WALLET_ADULT);
    RC_ASSERT(!rando.durabilityLowers, 20,
              "a rando file's Razor Sword still loses durability - ~100 hits break the seed's Razor (#1215)");
    RC_ASSERT(rando.walletFlagSet && rando.offered == GI_WALLET_ADULT, 20,
              "a rando file's day-2 Oceanside reward did not set the wallet check's flag - the reward is lost for "
              "the cycle (#1221)");
    RC_ASSERT(!rando.movedInStays, 20,
              "a rando file kept BUYER_MOVED_IN before the reward was collected - the man leaves the entrance and "
              "the Oceanside reward is lost for the cycle (#1221)");
    RC_ASSERT(rando.movedInWithReward, 20,
              "a rando file did not move the squatter in once the reward was collected (#1221)");
    return 0;
}

// W6-CYCLE-CHEATS: kCycleCheatRows (registered with the W6 list after Phase 4) in Phase 5's probe shape.
const std::vector<SkipKeyProbe>& CycleCheatProbes() {
    static const std::vector<SkipKeyProbe> probes = []() {
        std::vector<SkipKeyProbe> out;
        for (const CycleCheatRow& r : kCycleCheatRows) {
            out.push_back(SkipKeyProbe{ r.key, r.row, { SkipRegistry{ r.what, r.count, r.expected } } });
        }
        return out;
    }();
    return probes;
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
        AudioProbeKeyOff(probe);
    }
    // Phase 8: the same for Free Look.
    for (const auto& probe : FreeLookProbes()) {
        CVarSetInteger(probe.key, 0);
    }
    // Phase 9: the same for the key-gap keys (#1030-#1036, #1038).
    for (const auto& probe : KeyGapProbes()) {
        CVarSetInteger(probe.key, 0);
    }
    // The W6 phases (#1233): the same for every registered lane table.
    ZeroW6PhaseKeys(W6Phases());

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

    // ---- #1024-#1026, #1239: the Audio Editor TUs ------------------------------
    // Same drive as the enhancement loop above: ONE key on, ShipInit::Init(key)
    // twice, every registry the key's TU writes to must move by exactly the
    // table's count.
    for (const auto& probe : AudioProbes()) {
        std::vector<size_t> before;
        for (const auto& reg : probe.registries) {
            before.push_back(reg.count());
        }
        AudioProbeKeyOn(probe);
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
        AudioProbeKeyOff(probe);
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

    // ---- #1030-#1034: the five key-gap rows --------------------------------------
    // Same drive as Free Look above.
    for (const auto& probe : KeyGapProbes()) {
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
                printf("[TEST] FAIL(17): %s went %zu -> %zu when %s was turned on and driven twice, expected "
                       "exactly +%zu - the TU(s) behind the \"%s\" row are not in the link (a plain 2ship_enh "
                       "archive drops a TU whose only anchor is a RegisterShipInitFunc), or one stacks a "
                       "registration per toggle, so the row SoH's menu draws does nothing or misbehaves in "
                       "Majora's Mask (#1030-#1034)\n",
                       reg.what, before[i], after, probe.key, reg.expected, probe.row);
                ResetKeyGapKeys();
                ResetFreeLookKeys();
                ResetAudioKeys();
                ResetEnhKeys();
                ResetSkipKeys();
                ResetCheatKeys();
                return 17;
            }
        }
        CVarSetInteger(probe.key, 0);
        ShipInit::Init(probe.key);
        for (const auto& reg : probe.registries) {
            (void)reg.count();
        }
    }
    ResetKeyGapKeys();

    // ---- #1233: the W6 phases, every registered lane table ------------------------
    if (const int rc = RunW6PhaseList(); rc != 0) {
        return rc;
    }

    // ---- #1236 (review H1): MM Enable Bombchu Drops converts only while Link has Bombchus --------
    if (const int rc = RunChuDropsRuleLeg(); rc != 0) {
        return rc;
    }

    // ---- #1215, #1221: the rando-forced gates the link dropped -------------------
    // The whole SaveContext is saved and copied back, and the "IS_RANDO" drive
    // re-runs under the restored saveType, on pass and on fail.
    {
        auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
        memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
        memset(&gSaveContext, 0, sizeof(gSaveContext));
        const int rc = RunRandoGateLegs();
        memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));
        ShipInit::Init("IS_RANDO");
        if (rc != 0) {
            return rc;
        }
    }

    printf("[TEST] mm-registrar-coverage: PASS (4 re-homed registrars populated their registries; the two "
           "Phase-3 ExtensionCache entries ran, in order; the 7 Cheats keys each armed one hook; the 5 "
           "Skips & Speed-ups keys each armed their TUs; the 9 enhancement keys of #1010-#1017 each armed "
           "theirs; the 3 Audio Editor keys of #1024-#1026 each armed theirs; Free Look "
           "of #1023 armed both its hooks; the 10 key-gap keys of #1030-#1038 and #1123 each armed their TUs; "
           "the three rando-forced gates of #1215/#1221 arm under rando and answer as upstream)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
