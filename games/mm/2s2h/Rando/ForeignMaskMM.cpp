/**
 * @file ForeignMaskMM.cpp
 * @brief MM wears and draws an Ocarina of Time mask (#977, epic #975 P3).
 *
 * [lock] SEAM: today's behaviour. MM has no worn-mask field or slot choice yet,
 * so nothing is worn, put on or kept, no shared slot cycles, every C button
 * equips its own mask and every cell shows its own icon. The fix commit
 * replaces these bodies.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstdint>

#include "Rando/ForeignMaskMM.h"
#include "foreign_masks.h" // src/common

extern "C" uint8_t MM_ForeignMask_Worn(void) {
    return COMBO_FOREIGN_MASK_NONE;
}

extern "C" int MM_ForeignMask_Use(struct Player* player, uint8_t mask) {
    (void)player;
    (void)mask;
    return 0;
}

extern "C" void MM_ForeignMask_ApplyRules(struct Player* player) {
    (void)player;
}

extern "C" uint8_t MM_ForeignMask_SlotSelection(uint8_t slot) {
    (void)slot;
    return COMBO_FOREIGN_MASK_NONE;
}

extern "C" int MM_ForeignMask_CycleSlot(uint8_t slot, int dir) {
    (void)slot;
    (void)dir;
    return 0;
}

extern "C" int MM_ForeignMask_SlotVariantCount(uint8_t slot) {
    (void)slot;
    return 0;
}

extern "C" int MM_ForeignMask_OnEquipNative(struct Player* player, int maskId) {
    (void)player;
    (void)maskId;
    return 0;
}

extern "C" const char* MM_ForeignMask_PageIcon(int maskSlot, const char* nativeIcon) {
    (void)maskSlot;
    return nativeIcon;
}

extern "C" const char* MM_ForeignMask_WornDl(struct Player* player) {
    (void)player;
    return nullptr;
}

extern "C" uint8_t MM_ForeignMask_StoredRaw(void) {
    return COMBO_FOREIGN_MASK_NONE;
}

extern "C" void MM_ForeignMask_SetStoredRaw(uint8_t value) {
    (void)value;
}

extern "C" uint8_t MM_ForeignMask_SlotSelRaw(uint8_t slot) {
    (void)slot;
    return COMBO_FOREIGN_MASK_NONE;
}

extern "C" void MM_ForeignMask_SetSlotSelRaw(uint8_t slot, uint8_t value) {
    (void)slot;
    (void)value;
}

extern "C" size_t MM_ForeignMask_FieldOffset(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
