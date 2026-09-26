/**
 * @file mm_majora_goal_test.cpp
 * ROM-free, display-free lock for #658: MM_GOAL's "Majora defeated" conjunct
 * (ADR 0010 D1, docs/adr/0010-cross-game-logic-and-beatability.md:186-200).
 * CTest label "redship", row MMMajoraGoal in CMake/SingleExecutable.cmake,
 * dispatch "mm-majora-goal" in src/common/test_runner.cpp.
 *
 * WHAT WAS MISSING. `RR_MOON_MAJORAS_LAIR` was a reachable region carrying a
 * TODO where the fight belongs, and `CanKillEnemy`'s switch had no case for
 * Majora's actor -- asking it fell through to the default branch's assert
 * instead of answering. So "the lair is reachable" was the strongest goal fact
 * the region graph could yield, which is strictly weaker than the goal ADR 0010
 * D1 specifies. `Rando::Logic::CanDefeatMajora()` and
 * `Rando::Logic::MmGoalMajoraDefeated()` (Rando/Logic/Logic.h) close that.
 *
 * NON-VACUITY -- which legs are exact against the pre-#658 tree. An
 * empty-inventory leg is NOT: with NDEBUG the old default branch returned false
 * too, so "false with nothing" passed before the change. The legs that go red
 * without the ACTOR_BOSS_07 row are the POSITIVE ones (leg 3's sword, Goron,
 * Zora, magic-Deku, explosive and Deku-stick kits -- the stick under
 * MMRT_DEKU_STICK_FIGHTING since #719 -- and leg 6's composed goal):
 * the old switch answered false for every one of them. Verified red that way
 * before green.
 *
 * WHY THE KITS ARE WHAT THEY ARE. Authored from the fight's own tables and
 * handlers in games/mm/src/overlays/actors/ovl_Boss_07/z_boss_07.c, not from
 * feel. All three phases are stun-then-damage, and Wrath -- the binding phase --
 * is the only one that filters which attack may deal the damage: while stunned,
 * anything other than ANIM_FRAME_CHECK / DAMAGE_NONE / BLUE_LIGHT_ORB /
 * EXPLOSIVE re-stuns instead. In sWrathDmgTable those effects are carried by
 * Deku stick, Goron punch, Goron spikes, Deku spin, Deku launch, Zora punch,
 * sword, spin attack, sword beam and explosives, and NOT by arrows of any kind;
 * the hookshot is IMMUNE in every phase's table. Leg 4 is that fact as an
 * assertion: a bow-only or hookshot-only kit must answer false, because it can
 * stun Majora forever and never kill it. Leg 4 is also what stops a future
 * "simplification" to CAN_USE_PROJECTILE.
 *
 * LEG 2 IS THE FILL-INERTNESS LOCK, AND IT IS THE POINT OF THIS INCREMENT.
 * ADR 0010 increment 3 (#645), not this one, is where the fill consumes the
 * goal. Both the glitchless fill (Rando/Logic/GlitchlessLogic.cpp) and the
 * factored crawl (Rando/Logic/Logic.cpp) read the SAME RandoRegion struct, and
 * in the fill an event that first fires in an iteration where nothing else
 * changed takes the eventsInLogicChanged branch instead of the junk-swap branch
 * and bumps `weight` -- which feeds the cumulative-weight Ship_Random draw that
 * picks the check to re-roll. Adding the win as a region event or check here
 * would therefore move placements in every generated world. Leg 2 asserts
 * RR_MOON_MAJORAS_LAIR still carries exactly its two pots, no events, and its
 * one-way entrance, so that a later edit which quietly makes the goal a region
 * term goes red here rather than silently re-rolling every seed.
 *
 * SAVE DISCIPLINE. Every leg writes gSaveContext directly (forms, equipment,
 * inventory, magic, rando options and randoInf bits). The whole body is
 * bracketed by a memcpy snapshot/restore, so the process is left byte-identical
 * -- `--test all` shares one process and a leaked Fierce Deity form or an armed
 * boss-soul option would follow every later row. randoInf bits are written
 * directly rather than through Flags_SetRandoInf on purpose: the setter
 * dispatches OnFlagSet hooks, and this row must not fire game hooks.
 *
 * WHY PLAYER FORM IS SET EXPLICITLY. PLAYER_FORM_FIERCE_DEITY is 0
 * (z64player.h), so a zeroed save reads as Fierce Deity, which satisfies
 * CAN_USE_SWORD. Every leg that means "no kit" must therefore write
 * PLAYER_FORM_HUMAN first; a leg that forgot would pass for the wrong reason.
 *
 * WHAT THIS DOES NOT COVER. That the authored kits are SUFFICIENT in play --
 * that a Goron-only or stick-only run really can land the stun-then-damage
 * cycle on all three phases at the HP the tables give them -- needs a real
 * fight, and no headless row reaches it. It is an operator playtest. What is
 * locked here is that the predicate answers from the save at all, that its
 * polarity matches the damage tables, and that it stays out of the fill.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <set>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * MM's rando logic graph is only brought up through the single-exe
 * MM_Rando_InitCore path this row drives. A standalone 2ship build has its own
 * boot for it; test_runner.cpp declares this dispatch unconditionally, so the
 * row reports pass rather than failing to link.
 */
extern "C" int MM_MajoraGoal_RunHeadless(void) {
    printf("[TEST] mm-majora-goal: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <ship/Context.h>

#include "2s2h/Rando/Logic/Logic.h"

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;

// The CORE half of MM's rando bring-up (GameExports_SingleExe.cpp): ShipInit
// registrars + Rando::Init, which is what populates Rando::Logic::Regions.
// Asset-free and once-guarded; calling it twice is a no-op.
void MM_Rando_InitCore(void);
}

namespace {

#define MG_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// A save with no kit at all. PLAYER_FORM_HUMAN is written explicitly because
// PLAYER_FORM_FIERCE_DEITY is 0 and a memset save would otherwise read as
// Fierce Deity (see the file header).
void ClearKit() {
    memset(&gSaveContext.save.saveInfo.inventory, 0, sizeof(gSaveContext.save.saveInfo.inventory));
    memset(&gSaveContext.save.shipSaveInfo.rando.randoInf, 0, sizeof(gSaveContext.save.shipSaveInfo.rando.randoInf));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = false;
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_BOSS_SOULS] = RO_GENERIC_NO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_SOULS] = RO_GENERIC_NO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = RO_GENERIC_OFF;
}

void GiveInventoryItem(s32 item) {
    INV_CONTENT(item) = (u8)item;
}

// randoInf written directly: Flags_SetRandoInf dispatches OnFlagSet hooks, and
// this row must not fire game hooks.
void SetRandoInfBit(s32 flag, bool on) {
    u16& word = gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4];
    if (on) {
        word |= (u16)(1 << (flag & 0xF));
    } else {
        word &= (u16) ~(1 << (flag & 0xF));
    }
}

Rando::Logic::ReachabilityCrawl CrawlWithLair(bool includeLair) {
    Rando::Logic::ReachabilityCrawl crawl;
    crawl.reachableRegions.insert(RR_MOON);
    if (includeLair) {
        crawl.reachableRegions.insert(RR_MOON_MAJORAS_LAIR);
    }
    return crawl;
}

int RunLegs() {
    using Rando::Logic::CanDefeatMajora;
    using Rando::Logic::MmGoalMajoraDefeated;
    using Rando::Logic::Regions;

    // ---- Leg 1: the region graph is actually populated ---------------------
    // Without this every later leg that reads Regions would pass vacuously.
    MM_Rando_InitCore();
    MG_ASSERT(!Regions.empty(), 1,
              "Rando::Logic::Regions is empty after MM_Rando_InitCore - the registrars did not run, and leg 2 "
              "would be vacuous");
    MG_ASSERT(Regions.find(RR_MOON_MAJORAS_LAIR) != Regions.end(), 1,
              "RR_MOON_MAJORAS_LAIR is absent from the region graph - Regions/Moon.cpp's registrar did not run");

    // ---- Leg 2: the goal is NOT a fill input yet (#645 is) -----------------
    // See the file header. If a later edit makes the win a region event or
    // check, every generated world moves; this is where that goes red.
    {
        const auto& lair = Regions[RR_MOON_MAJORAS_LAIR];
        MG_ASSERT(lair.events.empty(), 2,
                  "RR_MOON_MAJORAS_LAIR gained a region EVENT - both the glitchless fill and the factored crawl "
                  "read this struct, and an event firing in an otherwise-idle fill iteration bumps `weight` and "
                  "moves the weighted Ship_Random junk draw, i.e. every generated world (#658 / ADR 0010 inc. 3)");
        MG_ASSERT(lair.checks.size() == 2, 2,
                  "RR_MOON_MAJORAS_LAIR's check set changed size - the two Majora pots are the whole set the fill "
                  "may see here; a goal term added as a CHECK moves placements the same way an event does");
        MG_ASSERT(lair.connections.empty(), 2,
                  "RR_MOON_MAJORAS_LAIR gained a region CONNECTION - the lair is a one-way terminal, and a new "
                  "edge out of it changes the crawl's reachable set");
        MG_ASSERT(lair.oneWayEntrances.size() == 1, 2,
                  "RR_MOON_MAJORAS_LAIR's one-way entrance set changed - lair reachability must stay exactly "
                  "where MM authored it (ADR 0010 D1.2: the goal's parameters are MM's own settings)");
    }

    // ---- Leg 3: polarity, empty kit then each authored kit alone -----------
    ClearKit();
    MG_ASSERT(!CanDefeatMajora(), 3, "Majora is defeatable with an empty inventory in human form");

    ClearKit();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    MG_ASSERT(CanDefeatMajora(), 3,
              "a Kokiri sword alone cannot defeat Majora - sword is ANIM_FRAME_CHECK and spin attack is "
              "DAMAGE_NONE in sWrathDmgTable, both of which damage Wrath while stunned");

    ClearKit();
    GiveInventoryItem(ITEM_MASK_GORON);
    MG_ASSERT(CanDefeatMajora(), 3,
              "the Goron mask alone cannot defeat Majora - Goron punch and Goron spikes are ANIM_FRAME_CHECK on "
              "Wrath and deal nonzero damage in all three phases' tables");

    ClearKit();
    GiveInventoryItem(ITEM_MASK_ZORA);
    MG_ASSERT(CanDefeatMajora(), 3,
              "the Zora mask alone cannot defeat Majora - Zora punch is ANIM_FRAME_CHECK on Wrath");

    ClearKit();
    GiveInventoryItem(ITEM_MASK_DEKU);
    MG_ASSERT(!CanDefeatMajora(), 3,
              "the Deku mask WITHOUT magic defeats Majora - the Deku attack that survives Wrath's filter is the "
              "spin, which costs magic");
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
    MG_ASSERT(CanDefeatMajora(), 3,
              "the Deku mask WITH magic cannot defeat Majora - the Deku spin is "
              "ANIM_FRAME_CHECK on Wrath");

    ClearKit();
    GiveInventoryItem(ITEM_BOMB);
    MG_ASSERT(CanDefeatMajora(), 3,
              "bombs alone cannot defeat Majora - explosives carry the EXPLOSIVE effect, one of the four Wrath "
              "accepts while stunned");

    // The Deku stick deals the damage (ANIM_FRAME_CHECK on Wrath), but since #719
    // Glitchless only COUNTS it as a weapon under MMRT_DEKU_STICK_FIGHTING, the
    // default-off OoTMM trick: a stick-only Majora kill is the trick, not the
    // baseline. Both halves are asserted, so neither the gate nor the row's stick
    // term can go missing unnoticed.
    ClearKit();
    GiveInventoryItem(ITEM_DEKU_STICK);
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[MMRT_DEKU_STICK_FIGHTING] = 0;
    MG_ASSERT(!CanDefeatMajora(), 3,
              "a Deku stick alone defeats Majora with MMRT_DEKU_STICK_FIGHTING OFF - stick combat is a default-off "
              "trick (#719), so tricks-off Glitchless must not assume it");
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[MMRT_DEKU_STICK_FIGHTING] = 1;
    MG_ASSERT(CanDefeatMajora(), 3,
              "a Deku stick alone cannot defeat Majora with MMRT_DEKU_STICK_FIGHTING ON - Deku stick is "
              "ANIM_FRAME_CHECK on Wrath");
    gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[MMRT_DEKU_STICK_FIGHTING] = 0;

    // ---- Leg 4: the stun-only kits, which is the fight's real constraint ---
    ClearKit();
    GiveInventoryItem(ITEM_BOW);
    MG_ASSERT(!CanDefeatMajora(), 4,
              "a bow alone defeats Majora - every arrow type is stun-only on Wrath (STUN_NONE / FIRE / FREEZE / "
              "LIGHT_ORB), so a bow-only kit can stun Majora forever and never kill it. This row must not be "
              "widened to CAN_USE_PROJECTILE");
    GiveInventoryItem(ITEM_ARROW_LIGHT);
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
    MG_ASSERT(!CanDefeatMajora(), 4,
              "light arrows defeat Majora - LIGHT_ORB stuns Wrath, it does not damage it (z_boss_07.c's Wrath "
              "stunned-branch filter)");

    ClearKit();
    GiveInventoryItem(ITEM_HOOKSHOT);
    MG_ASSERT(!CanDefeatMajora(), 4,
              "the hookshot alone defeats Majora - the hookshot is IMMUNE in all three phases' damage tables");

    // ---- Leg 5: the soul gate mirrors shouldMajoraRegister() ---------------
    // Boss-soul shuffle and the Triforce hunt EACH arm Majora's soul
    // (Rando/ActorBehavior/Souls.cpp); in hunt mode the soul is the grant that
    // blocks beating the game before the required piece count (GiveItem.cpp).
    ClearKit();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_BOSS_SOULS] = RO_GENERIC_YES;
    MG_ASSERT(!CanDefeatMajora(), 5,
              "boss-soul shuffle is on and Majora's soul is unfound, yet Majora is defeatable - the soul gate is "
              "not armed");
    SetRandoInfBit(RANDO_INF_OBTAINED_SOUL_OF_BOSS_MAJORA, true);
    MG_ASSERT(CanDefeatMajora(), 5, "Majora's soul is found under boss-soul shuffle, yet Majora is not defeatable");

    ClearKit();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = RO_GENERIC_YES;
    MG_ASSERT(!CanDefeatMajora(), 5,
              "the Triforce hunt is on and Majora's soul is ungranted, yet Majora is defeatable - the hunt arms "
              "the soul too (shouldMajoraRegister), and the soul is what blocks an early win");
    SetRandoInfBit(RANDO_INF_OBTAINED_SOUL_OF_BOSS_MAJORA, true);
    MG_ASSERT(CanDefeatMajora(), 5,
              "the Triforce hunt granted Majora's soul at the required piece count, yet Majora is not defeatable");

    // Enemy-soul shuffle must NOT gate a boss: ACTOR_BOSS_07 is absent from
    // enemySoulMap, so HaveEnemySoul answers true. This leg goes red if someone
    // adds Majora to that map without revisiting this row.
    ClearKit();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_ENEMY_SOULS] = RO_GENERIC_YES;
    MG_ASSERT(CanDefeatMajora(), 5,
              "enemy-soul shuffle blocks Majora - Majora is a BOSS soul, absent from enemySoulMap, and must not "
              "be gated by the enemy-soul option");

    // ---- Leg 6: MM_GOAL composes both of ADR 0010 D1's conjuncts -----------
    ClearKit();
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
    MG_ASSERT(!MmGoalMajoraDefeated(CrawlWithLair(false)), 6,
              "MM_GOAL is satisfied with the full kit but WITHOUT the lair reachable - the reachability conjunct "
              "is missing");
    MG_ASSERT(MmGoalMajoraDefeated(CrawlWithLair(true)), 6,
              "MM_GOAL is unsatisfied with the lair reachable AND the kit in hand");

    ClearKit();
    MG_ASSERT(!MmGoalMajoraDefeated(CrawlWithLair(true)), 6,
              "MM_GOAL is satisfied with the lair reachable and an EMPTY kit - this is exactly the pre-#658 "
              "state the issue is about: lair reachability standing in for the goal");

    return 0;
}

} // namespace

extern "C" int MM_MajoraGoal_RunHeadless(void) {
    printf("[TEST] mm-majora-goal: MM_GOAL's \"Majora defeated\" conjunct answers from the save, matches the "
           "fight's damage tables, and stays out of the fill (#658)\n");

    auto ctx = Ship::Context::GetInstance();
    MG_ASSERT(ctx != nullptr, 7, "Ship::Context singleton missing - run the shared bring-up first");
    MG_ASSERT(ctx->GetConsoleVariables() != nullptr, 7,
              "ConsoleVariables missing - MM_Rando_InitCore's registrars read them");

    // Snapshot/restore bracket: --test all shares one process, and every leg
    // writes gSaveContext.
    static SaveContext sSaved;
    memcpy(&sSaved, &gSaveContext, sizeof(SaveContext));
    int rc = RunLegs();
    memcpy(&gSaveContext, &sSaved, sizeof(SaveContext));

    if (rc == 0) {
        printf("[TEST] mm-majora-goal: PASS (predicate polarity matches all three phases' damage tables, the "
               "soul gate mirrors shouldMajoraRegister, and the lair region carries no fill-visible goal term)\n");
    }
    return rc;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
