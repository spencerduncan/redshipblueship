/**
 * @file mm_foreign_mask_test.cpp
 * MM's half of the cross-game worn-mask locks (#977; the shared rules are
 * src/common/foreign_masks.h, MM's are 2s2h/Rando/ForeignMaskMM.cpp).
 *
 * THE HELPERS the redship rows `foreign-mask-rules` and
 * `foreign-mask-persistence` (src/common/tests/test_foreign_masks.c) drive MM's
 * REAL rule functions and freeze/consume calls through. They only put MM's live
 * save and a heap Player in a known state and read them back; every decision
 * is the production code's. They live here because only an MM TU has MM's
 * `gSaveContext`, `Player` and inventory macros in scope. The worn field and the
 * slot choices are read and written through ForeignMaskMM.h's raw accessors.
 */

#include "global.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

extern "C" {
#include "z64save.h"
extern SaveContext gSaveContext;
}
#include "Rando/ForeignMaskMM.h"

#include "context.h"  // src/common: Combo_FreezeState, Combo_ConsumeFrozenState
#include "entrance.h" // src/common: MM_ENTR_SOUTH_CLOCK_TOWN_0

extern "C" int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);

namespace {
// A heap Player: the rule functions read and write transformation, currentMask
// and prevMask.
std::unique_ptr<Player> sPlayer;

Player* TestPlayer() {
    if (!sPlayer) {
        sPlayer = std::make_unique<Player>();
        std::memset(sPlayer.get(), 0, sizeof(Player));
        sPlayer->transformation = PLAYER_FORM_HUMAN;
    }
    return sPlayer.get();
}
} // namespace

/** MM's live save as a cross-game MM session runs it, human, all else zero. */
extern "C" void MM_ForeignMaskTest_ArmLive(void) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    for (int i = 0; i < MASK_NUM_SLOTS; i++) {
        gSaveContext.save.saveInfo.inventory.items[i + ITEM_NUM_SLOTS] = ITEM_NONE;
    }
}

extern "C" void MM_ForeignMaskTest_SetStored(uint8_t value) {
    MM_ForeignMask_SetStoredRaw(value);
}

extern "C" uint8_t MM_ForeignMaskTest_Stored(void) {
    return MM_ForeignMask_StoredRaw();
}

/** The worn mask as every MM reader sees it (sanitized, rule-gated). */
extern "C" uint8_t MM_ForeignMaskTest_Worn(void) {
    return MM_ForeignMask_Worn();
}

extern "C" void MM_ForeignMaskTest_SetSlotSel(uint8_t slot, uint8_t value) {
    MM_ForeignMask_SetSlotSelRaw(slot, value);
}

extern "C" uint8_t MM_ForeignMaskTest_SlotSelStored(uint8_t slot) {
    return MM_ForeignMask_SlotSelRaw(slot);
}

extern "C" uint8_t MM_ForeignMaskTest_SlotSelection(uint8_t slot) {
    return MM_ForeignMask_SlotSelection(slot);
}

/** Own (or not) a shared slot's own mask: 1 = Stone's slot, 2 = Gibdo's. */
extern "C" void MM_ForeignMaskTest_SetNativeOwned(uint8_t slot, int owned) {
    const int item = slot == 1 ? ITEM_MASK_STONE : ITEM_MASK_GIBDO;
    INV_CONTENT(item) = owned ? (u8)item : (u8)ITEM_NONE;
}

/** The mask-page cell (0-based) a shared slot's own mask sits in. */
extern "C" int MM_ForeignMaskTest_PageCell(uint8_t slot) {
    const int item = slot == 1 ? ITEM_MASK_STONE : ITEM_MASK_GIBDO;
    return SLOT(item) - ITEM_NUM_SLOTS;
}

extern "C" const char* MM_ForeignMaskTest_PageIcon(int cell, const char* nativeIcon) {
    return MM_ForeignMask_PageIcon(cell, nativeIcon);
}

extern "C" void MM_ForeignMaskTest_SetForm(int human) {
    TestPlayer()->transformation = human ? PLAYER_FORM_HUMAN : PLAYER_FORM_DEKU;
}

extern "C" void MM_ForeignMaskTest_SetPlayerMask(uint8_t mask) {
    TestPlayer()->currentMask = mask;
}

extern "C" int MM_ForeignMaskTest_PlayerMask(void) {
    return (int)TestPlayer()->currentMask;
}

extern "C" void MM_ForeignMaskTest_SetEquippedMask(uint8_t mask) {
    gSaveContext.save.equippedMask = mask;
}

extern "C" int MM_ForeignMaskTest_EquippedMask(void) {
    return (int)gSaveContext.save.equippedMask;
}

/** MM's own bound on currentMask: PLAYER_MASK_MAX. */
extern "C" int MM_ForeignMaskTest_PlayerMaskMax(void) {
    return (int)PLAYER_MASK_MAX;
}

extern "C" int MM_ForeignMaskTest_MaskStone(void) {
    return (int)PLAYER_MASK_STONE;
}

extern "C" int MM_ForeignMaskTest_MaskGibdo(void) {
    return (int)PLAYER_MASK_GIBDO;
}

extern "C" int MM_ForeignMaskTest_MaskKeaton(void) {
    return (int)PLAYER_MASK_KEATON;
}

extern "C" int MM_ForeignMaskTest_Use(uint8_t mask) {
    return MM_ForeignMask_Use(TestPlayer(), mask);
}

extern "C" void MM_ForeignMaskTest_ApplyRules(void) {
    MM_ForeignMask_ApplyRules(TestPlayer());
}

extern "C" int MM_ForeignMaskTest_CycleSlot(uint8_t slot, int dir) {
    return MM_ForeignMask_CycleSlot(slot, dir);
}

extern "C" int MM_ForeignMaskTest_EquipNative(int maskId) {
    return MM_ForeignMask_OnEquipNative(TestPlayer(), maskId);
}

extern "C" const char* MM_ForeignMaskTest_WornDl(void) {
    return MM_ForeignMask_WornDl(TestPlayer());
}

extern "C" size_t MM_ForeignMaskTest_FieldOffset(void) {
    return MM_ForeignMask_FieldOffset();
}

/** The member the fields are appended after: RandoSaveInfo's last before #977. */
extern "C" size_t MM_ForeignMaskTest_PrevLastOffset(void) {
    return offsetof(SaveContext, save) + offsetof(Save, shipSaveInfo) + offsetof(ShipSaveInfo, rando) +
           offsetof(RandoSaveInfo, randoSaveExcludedChecks);
}

extern "C" size_t MM_ForeignMaskTest_SaveSize(void) {
    return sizeof(Save);
}

/** A crossing's freeze of the live MM half. */
extern "C" void MM_ForeignMaskTest_Freeze(void) {
    Combo_FreezeState("mm", MM_ENTR_SOUTH_CLOCK_TOWN_0, &gSaveContext, sizeof(gSaveContext));
}

/** z_play.c's arrival consume of the frozen MM half into the live save. */
extern "C" int MM_ForeignMaskTest_Consume(void) {
    return Combo_ConsumeFrozenState("mm", &gSaveContext, sizeof(gSaveContext));
}
