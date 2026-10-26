/**
 * @file oot_shared_bombchu_test.cpp
 * OoT's half of the shared-bombchu pickup lock (#1061).
 *
 * THE SHIM HELPERS the redship row `shared-bombchu-pickup`
 * (src/common/tests/test_shared_bombchu.c) drives OoT's REAL
 * OoT_HarvestSharedResources / OoT_ApplySharedResources and the REAL pickup
 * core OoT_Rando_Foreign_RecordPickup through. They only put OoT's live save in
 * a known state and read the bombchu slot back; every merge decision is the
 * production code's.
 *
 * The live save is a VANILLA (QUEST_NORMAL) file: the bombchu row is shared for
 * every world, and outside the randomizer OoT's bombchu ceiling is the fixed 50
 * with no bag gate (OoT_BombchuCapacity, OoT_SharedAmmoGrantBlocked), so the row
 * needs no Randomizer context and reads one deterministic cap.
 */

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/OTRGlobals.h"

#include <cstdio>
#include <memory>

// z_en_item00.c: the drop conversion every collectible drop runs (#1164).
extern "C" s16 func_8001F404(s16 dropId);

/** OoT's live save as a real loaded file holding the bombchu item and @p count
 *  bombchus (the bag owned). GAMEMODE_NORMAL is what Combo_SaveIsLiveFile
 *  requires before the harvest or a pickup credit does anything. Everything
 *  else zero, so the harvest offers no other kind a nonzero value. */
extern "C" void OoT_SharedBombchuTest_ArmLive(int count) {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
    AMMO(ITEM_BOMBCHU) = (s8)count;
}

/** Set OoT's live bombchu count (what the player holds mid-stint). */
extern "C" void OoT_SharedBombchuTest_SetCount(int count) {
    AMMO(ITEM_BOMBCHU) = (s8)count;
}

/** OoT's live bombchu count. */
extern "C" int OoT_SharedBombchuTest_Count(void) {
    return (int)AMMO(ITEM_BOMBCHU);
}

/** OoT's own id for a 10-bombchu pack (RG_BOMBCHU_10). */
extern "C" int OoT_SharedBombchuTest_PackId(void) {
    return (int)RG_BOMBCHU_10;
}

/**
 * #1164 GREEN PIN, the rando-tier row `bombchu-drop-rule`: SoH's own drop rule
 * (z_en_item00.c func_8001F404, vendored and unmodified) turns a bomb drop into
 * a bombchu drop when Bombchu Drops is on and either the Bombchu Bag setting is
 * not None or a bomb bag is held. With no bomb bag the conversion picks the
 * bombchu (EnItem00_ConvertBombDropToBombchu). So under Single Bag the bag alone
 * enables chu drops; under None, with no bomb bag, a bomb drop is cancelled.
 *
 * Drives the real function against OTRGlobals' randomizer context (the row's
 * headless bring-up creates it), with OoT's live save a child randomizer file.
 * Both settings and the whole save are put back before returning; no CVar is
 * read for a decision here or written at all (the Bombchu Drops enhancement CVar
 * only widens the first clause, which both legs already satisfy).
 *
 * @return 0 when both legs hold, 1 otherwise.
 */
extern "C" int OoT_BombchuDropRuleTest_Run(void) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        std::printf("[TEST] FAIL: bombchu-drop-rule needs OTRGlobals' randomizer context\n");
        return 1;
    }
    const std::shared_ptr<Rando::Context> ctx = OTRGlobals::Instance->gRandoContext;
    const uint8_t priorDrops = ctx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Get();
    const uint8_t priorBag = ctx->GetOption(RSK_BOMBCHU_BAG).Get();
    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));

    int failures = 0;
    static const struct {
        uint8_t bag;
        const char* name;
        s16 expected;
    } kLegs[] = {
        { RO_BOMBCHU_BAG_SINGLE, "Single Bag", ITEM00_BOMBCHU },
        { RO_BOMBCHU_BAG_NONE, "None", -1 },
    };
    for (const auto& leg : kLegs) {
        std::memset(&gSaveContext, 0, sizeof(SaveContext));
        std::memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
        gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
        gSaveContext.linkAge = LINK_AGE_CHILD;
        gSaveContext.healthCapacity = 0x30;
        gSaveContext.health = 0x30;
        if (leg.bag != RO_BOMBCHU_BAG_NONE) {
            // The Bombchu Bag found (SoH's own give: the item and 20 chus).
            INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            AMMO(ITEM_BOMBCHU) = 20;
        }
        ctx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Set(1);
        ctx->GetOption(RSK_BOMBCHU_BAG).Set(leg.bag);
        const s16 got = func_8001F404(ITEM00_BOMBS_A);
        std::printf("[TEST] bombchu-drop-rule: Bombchu Drops on, Bombchu Bag %s, no bomb bag: a bomb drop becomes %d "
                    "(expected %d)\n",
                    leg.name, (int)got, (int)leg.expected);
        if (got != leg.expected) {
            std::printf("[TEST] FAIL: under Bombchu Bag %s with no bomb bag a bomb drop became %d, expected %d\n",
                        leg.name, (int)got, (int)leg.expected);
            failures++;
        }
    }

    ctx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Set(priorDrops);
    ctx->GetOption(RSK_BOMBCHU_BAG).Set(priorBag);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    return failures == 0 ? 0 : 1;
}
