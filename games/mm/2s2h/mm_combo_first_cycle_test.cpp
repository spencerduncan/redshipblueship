/**
 * ROM-free lock for "the cross-game arrival IS Majora's Mask's intro event"
 * (#654, operator ruling 2026-09-16). CTest label "redship", row
 * mm-combo-first-cycle in src/common/test_runner.cpp.
 *
 * WHAT #654 REPORTED. A combo MM half arrived in Clock Town with no Ocarina of
 * Time, because the arrival skips the intro that grants it. Vanilla MM reads
 * "no ocarina" as "the intro has not happened yet" and loads Termina Field's
 * FIRST-CYCLE EMPTY layer (scene layer 5: no enemies, and a different
 * SCENE_CMD_SOUND_SETTINGS, so no BGM) on every visit — and with no ocarina
 * there is no Song of Time either, so the cycle can never be reset and the
 * field is empty forever. The only override in the tree was
 * `COND_VB_SHOULD(VB_TERMINA_FIELD_BE_EMPTY, IS_RANDO, ...)`
 * (Rando/MiscBehavior/MiscBehavior.cpp), which covers a paired RANDO half and
 * nothing else.
 *
 * THE THREE CONTRACTS THIS ROW LOCKS.
 *
 * 1. The GATE. MM_Play_ShouldEmptyFirstCycleTerminaField (games/mm/src/code/
 *    z_play.c) keeps vanilla's verdict for a standalone ocarina-less MM file and
 *    answers false for a combo MM half, through MM's real VB dispatcher and the
 *    real registrar. The baseline leg runs FIRST, with the arrival latch
 *    explicitly cleared, so "not empty" cannot pass by the gate never firing.
 *
 * 2. The GRANTS. MM_Play_GrantComboArrivalIntroRewards authors the post-intro
 *    start state and the intro rewards' vanilla contents for a vanilla-paired
 *    half, and authors NOTHING for a paired rando half (whose start state and
 *    item set are OnFileCreate's and the fill's).
 *
 * 3. The WIRING. MM_Play_ConsumeStartupEntrance runs the grants on the FIRST
 *    entry only. A restored return leg is the player's own session and must come
 *    back untouched — the same gate #639's clock re-author uses, and the reason
 *    mm-startup-restore's byte-exact assertion stays green.
 *
 * WHY A ZEROED SaveContext WOULD MAKE THIS VACUOUS, and what is used instead.
 * `ITEM_OCARINA_OF_TIME == 0x00` (z64item.h), so a memset-to-0 inventory reads
 * as "holding the ocarina" and the gate never fires at all. Every leg below
 * arranges the REAL bootstrap MM_Sram_InitNewSave authors (items filled with
 * ITEM_NONE == 0xFF), which is also exactly what a first cross-game entry
 * actually has.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdio>
#include <cstring>

#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
// games/mm/src/code/z_play.c — the three functions under test.
void MM_Play_ConsumeStartupEntrance(void);
void MM_Play_GrantComboArrivalIntroRewards(void);
bool MM_Play_ShouldEmptyFirstCycleTerminaField(s32 scene, u16 entrance);
// games/mm/2s2h/GameExports_SingleExe.cpp — the combo override registrar.
void MM_Combo_RegisterFirstCycleOverrides(void);
// src/common/context.cpp + entrance.cpp — the same C surface z_play.c uses.
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance, const void* saveContext, size_t size);
void Combo_ClearFrozenState(const char* gameId);
void Combo_SetStartupEntrance(uint16_t entrance);
bool Combo_HasStartupEntrance(void);
void Combo_ClearStartupEntrance(void);
void Combo_NoteCrossGameArrival(const char* gameId);
bool Combo_IsCrossGameHalf(const char* gameId);
void Combo_ClearCrossGameHalves(void);
}

namespace {

#define FC_ASSERT(cond, msg)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

// ENTRANCE(SOUTH_CLOCK_TOWN, 0) — the OoT -> MM arrival spawn, the same literal
// mm_resume_state_test.cpp uses.
constexpr uint16_t kArrival = 0xD800;

// The bootstrap save a first cross-game entry actually reaches Play_Init with:
// MM_Sram_InitNewSave's new file plus the title-screen attract demo's clock
// (TitleSetup_SetupTitleScreen, ovl_opening/z_opening.c). Same arrangement
// mm-startup-restore's first-entry leg uses, for the same reason.
void ArrangeBootstrapSave(void) {
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    MM_Sram_InitNewSave();
    gSaveContext.save.time = CLOCK_TIME(8, 0);
    gSaveContext.save.day = 1;
}

bool HasOcarina(void) {
    return INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
}

} // namespace

extern "C" int MM_ComboFirstCycle_RunHeadless(void) {
    printf("[TEST] mm-combo-first-cycle: the cross-game arrival is MM's intro event (#654)\n");

    // The registrar under test. Idempotent — MM_Rando_Init may already have run
    // it in this process (mm-registrar-coverage precedes this row under
    // `--test all`), and its own once-guard makes a second call a no-op.
    MM_Combo_RegisterFirstCycleOverrides();

    // ========================================================================
    // 1. The gate: vanilla's verdict survives for a NON-combo MM file.
    // ========================================================================
    Combo_ClearCrossGameHalves();
    ArrangeBootstrapSave();
    FC_ASSERT(!Combo_IsCrossGameHalf("mm"), "arrival latch not clear at the start of the baseline leg");
    FC_ASSERT(!HasOcarina(), "bootstrap save unexpectedly holds the ocarina — the gate could not fire");
    FC_ASSERT(MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, ENTRANCE(TERMINA_FIELD, 0)),
              "standalone ocarina-less MM file no longer gets vanilla's empty first-cycle Termina Field");
    // The two scope terms of the vanilla verdict, so a helper that ignored its
    // arguments could not pass the leg above.
    FC_ASSERT(!MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, ENTRANCE(TERMINA_FIELD, 10)),
              "empty-field verdict ignored the ENTRANCE(TERMINA_FIELD, 10) carve-out");
    FC_ASSERT(!MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_SOUTH_CLOCK_TOWN, ENTRANCE(TERMINA_FIELD, 0)),
              "empty-field verdict fired for a scene that is not Termina Field");
    // ...and the vanilla ocarina term itself.
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ITEM_OCARINA_OF_TIME;
    FC_ASSERT(!MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, ENTRANCE(TERMINA_FIELD, 0)),
              "empty-field verdict fired for a file that holds the Ocarina of Time");

    // The faster-first-cycle twin, through MM's real dispatcher. Its production
    // consumer (MM_Scene_CommandTimeSettings, games/mm/2s2h/z_scene_2SH.cpp)
    // is #ifdef RSBS_SINGLE_EXECUTABLE-branched to evaluate the un-hooked
    // default directly and so does not reach this dispatch today (#344); what is
    // asserted here is the OVERRIDE, not that the scene command consults it.
    FC_ASSERT(GameInteractor_Should(VB_FASTER_FIRST_CYCLE, true),
              "VB_FASTER_FIRST_CYCLE no longer defaults to the caller's verdict off a combo half");

    // ========================================================================
    // 2. The gate: a COMBO MM half is never the empty field, ocarina or not.
    // ========================================================================
    ArrangeBootstrapSave();
    Combo_NoteCrossGameArrival("mm");
    FC_ASSERT(Combo_IsCrossGameHalf("mm"), "arrival latch not set by Combo_NoteCrossGameArrival");
    FC_ASSERT(!HasOcarina(), "combo-half leg lost its ocarina-less arrangement");
    FC_ASSERT(!MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, ENTRANCE(TERMINA_FIELD, 0)),
              "an ocarina-less COMBO MM half still gets the empty first-cycle Termina Field (#654)");
    FC_ASSERT(!GameInteractor_Should(VB_FASTER_FIRST_CYCLE, true),
              "a COMBO MM half still runs the first cycle's 5x clock (#654)");

    // ========================================================================
    // 3. The grants, driven directly: a VANILLA-paired half gets the post-intro
    //    start state and the intro rewards' vanilla contents.
    // ========================================================================
    ArrangeBootstrapSave();
    FC_ASSERT(gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_VANILLA,
              "bootstrap save is not SAVETYPE_VANILLA — the grant gate could not be exercised");
    FC_ASSERT(!gSaveContext.save.isFirstCycle && !gSaveContext.save.hasTatl,
              "bootstrap save already post-intro — the grants could not be observed");
    FC_ASSERT(INV_CONTENT(ITEM_MASK_DEKU) != ITEM_MASK_DEKU && INV_CONTENT(ITEM_DEKU_NUT) != ITEM_DEKU_NUT &&
                  !CHECK_QUEST_ITEM(QUEST_SONG_TIME) && !CHECK_QUEST_ITEM(QUEST_SONG_HEALING) &&
                  !gSaveContext.save.saveInfo.playerData.isMagicAcquired,
              "bootstrap save already holds intro rewards — the grant assertions would be vacuous");
    MM_Play_GrantComboArrivalIntroRewards();
    FC_ASSERT(HasOcarina(), "vanilla-paired arrival did not grant the Ocarina of Time (#654)");
    FC_ASSERT(INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU, "vanilla-paired arrival did not grant the Deku Mask");
    FC_ASSERT(CHECK_QUEST_ITEM(QUEST_SONG_TIME), "vanilla-paired arrival did not grant the Song of Time");
    FC_ASSERT(CHECK_QUEST_ITEM(QUEST_SONG_HEALING), "vanilla-paired arrival did not grant the Song of Healing");
    FC_ASSERT(gSaveContext.save.saveInfo.playerData.isMagicAcquired, "vanilla-paired arrival did not acquire magic");
    FC_ASSERT(gSaveContext.save.playerForm == PLAYER_FORM_HUMAN, "vanilla-paired arrival did not start in human form");
    FC_ASSERT(gSaveContext.save.hasTatl, "vanilla-paired arrival did not start with Tatl");
    FC_ASSERT(gSaveContext.save.saveInfo.playerData.threeDayResetCount == 1,
              "vanilla-paired arrival did not skip the first cycle (threeDayResetCount)");
    FC_ASSERT(gSaveContext.save.isFirstCycle, "vanilla-paired arrival did not mark isFirstCycle");
    // The Deku Nuts chest, opened, with the nuts lost to the form change — what
    // a post-intro vanilla file actually holds (SkipIntroSequence.cpp:34-47).
    FC_ASSERT((gSaveContext.cycleSceneFlags[SCENE_OPENINGDAN].chest & (1 << 0)) != 0,
              "vanilla-paired arrival did not mark the intro Deku Nuts chest opened");
    FC_ASSERT(INV_CONTENT(ITEM_DEKU_NUT) == ITEM_DEKU_NUT, "vanilla-paired arrival did not leave the nut slot filled");
    FC_ASSERT(AMMO(ITEM_DEKU_NUT) == 0, "vanilla-paired arrival kept the Deku Nuts the form change loses");
    // Sword and shield are KEPT — nothing is shuffled in a vanilla pairing.
    FC_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE,
              "vanilla-paired arrival left the MM half swordless");
    FC_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE,
              "vanilla-paired arrival left the MM half shieldless");

    // ...and the REFUSED-pairing shape: OnFileCreate strips sword and shield for
    // the shuffle before its generation throws, and its catch reverts the file to
    // SAVETYPE_VANILLA. A file that reaches these grants shuffles nothing, so the
    // strip is restored rather than inherited.
    ArrangeBootstrapSave();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    MM_Play_GrantComboArrivalIntroRewards();
    FC_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI,
              "a REFUSED pairing's stripped sword was not restored");
    FC_ASSERT(BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_SWORD_KOKIRI,
              "a REFUSED pairing's stripped sword was not re-equipped to B");
    FC_ASSERT(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HERO,
              "a REFUSED pairing's stripped shield was not restored");

    // ========================================================================
    // 4. The grants, driven directly: a paired RANDO half is left ALONE. Its
    //    start state is OnFileCreate's and its intro rewards are CHECKS whose
    //    contents the fill decides (ADR 0009, "Operator rulings 2026-09-16").
    // ========================================================================
    ArrangeBootstrapSave();
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    MM_Play_GrantComboArrivalIntroRewards();
    FC_ASSERT(!HasOcarina(), "the vanilla intro grants ran over a paired RANDO half (#654)");
    FC_ASSERT(INV_CONTENT(ITEM_MASK_DEKU) != ITEM_MASK_DEKU,
              "the vanilla Deku Mask grant ran over a paired RANDO half");
    FC_ASSERT(!CHECK_QUEST_ITEM(QUEST_SONG_TIME), "the vanilla Song of Time grant ran over a paired RANDO half");
    FC_ASSERT(!gSaveContext.save.saveInfo.playerData.isMagicAcquired,
              "the vanilla magic grant ran over a paired RANDO half");

    // ========================================================================
    // 5. The wiring: MM_Play_ConsumeStartupEntrance runs the grants on a FIRST
    //    entry and the arrival latch comes up with them.
    // ========================================================================
    Combo_ClearFrozenState("mm");
    Combo_ClearCrossGameHalves();
    ArrangeBootstrapSave();
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();
    FC_ASSERT(gSaveContext.save.entrance == kArrival, "first-entry startup entrance not applied");
    FC_ASSERT(!Combo_HasStartupEntrance(), "first-entry startup entrance not consumed");
    FC_ASSERT(Combo_IsCrossGameHalf("mm"), "the arrival did not note MM as a combo half (#654)");
    FC_ASSERT(HasOcarina(), "the first-entry arrival did not grant the Ocarina of Time (#654)");
    FC_ASSERT(INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU, "the first-entry arrival did not grant the Deku Mask");
    FC_ASSERT(CHECK_QUEST_ITEM(QUEST_SONG_TIME) && CHECK_QUEST_ITEM(QUEST_SONG_HEALING),
              "the first-entry arrival did not grant both intro songs");
    FC_ASSERT(gSaveContext.save.saveInfo.playerData.isMagicAcquired, "the first-entry arrival did not acquire magic");
    FC_ASSERT(gSaveContext.save.playerForm == PLAYER_FORM_HUMAN, "the first-entry arrival did not start in human form");
    FC_ASSERT(gSaveContext.save.hasTatl && gSaveContext.save.isFirstCycle &&
                  gSaveContext.save.saveInfo.playerData.threeDayResetCount == 1,
              "the first-entry arrival did not skip the first cycle");
    // The whole point, end to end: Termina Field is populated for this half.
    FC_ASSERT(!MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, ENTRANCE(TERMINA_FIELD, 0)),
              "Termina Field is still the empty first-cycle layer after a vanilla-paired arrival (#654)");

    // ========================================================================
    // 6. The wiring: a RESTORED return leg is the player's own session and gets
    //    NOTHING. This is the assertion that keeps mm-startup-restore's
    //    byte-exact restore contract true.
    // ========================================================================
    ArrangeBootstrapSave();
    // A mid-game MM session that genuinely has none of the intro rewards: the
    // player could have lost them to a moon crash, or simply never played the
    // intro on this file. Whatever the reason, the arrival must not re-author it.
    gSaveContext.save.playerForm = PLAYER_FORM_ZORA;
    gSaveContext.save.day = 2;
    Combo_FreezeState("mm", kArrival, &gSaveContext, sizeof(gSaveContext));
    memset(&gSaveContext, 0, sizeof(gSaveContext)); // the boot chain's wipe
    Combo_SetStartupEntrance(kArrival);
    MM_Play_ConsumeStartupEntrance();
    FC_ASSERT(gSaveContext.save.day == 2, "restored leg did not restore the frozen save at all");
    FC_ASSERT(!HasOcarina(), "a RESTORED return leg was granted the Ocarina of Time (#654)");
    FC_ASSERT(INV_CONTENT(ITEM_MASK_DEKU) != ITEM_MASK_DEKU, "a RESTORED return leg was granted the Deku Mask");
    FC_ASSERT(!CHECK_QUEST_ITEM(QUEST_SONG_TIME) && !CHECK_QUEST_ITEM(QUEST_SONG_HEALING),
              "a RESTORED return leg was granted the intro songs");
    FC_ASSERT(!gSaveContext.save.saveInfo.playerData.isMagicAcquired, "a RESTORED return leg was granted magic");
    FC_ASSERT(gSaveContext.save.playerForm == PLAYER_FORM_ZORA,
              "a RESTORED return leg had its player form re-authored to human");
    FC_ASSERT(gSaveContext.save.saveInfo.playerData.threeDayResetCount == 0,
              "a RESTORED return leg had its cycle count re-authored");
    // ...and it is still a combo half, so the first-cycle gates still apply to
    // it. That is the whole reason the override is a session latch and not a
    // consequence of the grants.
    FC_ASSERT(Combo_IsCrossGameHalf("mm"), "the return leg stopped being a combo half");
    FC_ASSERT(!MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, ENTRANCE(TERMINA_FIELD, 0)),
              "an ocarina-less RESTORED combo half still gets the empty Termina Field (#654)");

    // Leave clean global state for later rows.
    Combo_ClearFrozenState("mm");
    Combo_ClearStartupEntrance();
    Combo_ClearCrossGameHalves();
    memset(&gSaveContext, 0, sizeof(gSaveContext));

    printf("[TEST] PASS: mm-combo-first-cycle — the arrival is the intro event, first entry only\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
