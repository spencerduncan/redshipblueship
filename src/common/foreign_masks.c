/**
 * @file foreign_masks.c
 * @brief The cross-game worn-mask table and equip rules (#977). See foreign_masks.h.
 *
 * [lock] SEAM: today's behaviour. No game wears another's mask yet, so the
 * table has no row, nothing sanitizes to a mask, nothing is owned and no list
 * cycles. The fix commit replaces these bodies with the table and the rules.
 */

#include "foreign_masks.h"

#include <stddef.h>

const ComboForeignMaskInfo* Combo_ForeignMask_Info(uint8_t id) {
    (void)id;
    return NULL;
}

int Combo_ForeignMask_IsWornIn(uint8_t id, GameId host) {
    (void)id;
    (void)host;
    return 0;
}

int Combo_ForeignMask_Wearable(uint8_t id, GameId host) {
    (void)id;
    (void)host;
    return 0;
}

uint8_t Combo_ForeignMask_Sanitize(uint8_t value, GameId host) {
    (void)value;
    (void)host;
    return (uint8_t)COMBO_FOREIGN_MASK_NONE;
}

uint8_t Combo_ForeignMask_Toggle(uint8_t worn, uint8_t requested, GameId host) {
    (void)requested;
    (void)host;
    return worn;
}

void Combo_ForeignMask_SetOwnedSource(GameId host, ComboForeignMaskOwnedFn fn) {
    (void)host;
    (void)fn;
}

int Combo_ForeignMask_Owned(GameId host, uint8_t id) {
    (void)host;
    (void)id;
    return 0;
}

int Combo_ForeignMask_SlotVariants(uint8_t slot, int nativeOwned, uint8_t* out, int cap) {
    (void)slot;
    (void)nativeOwned;
    (void)out;
    (void)cap;
    return 0;
}

uint8_t Combo_ForeignMask_SlotCycle(uint8_t slot, uint8_t current, int nativeOwned, int dir) {
    (void)slot;
    (void)current;
    (void)nativeOwned;
    (void)dir;
    return (uint8_t)COMBO_FOREIGN_MASK_NONE;
}

int Combo_ListCycle_Step(const uint8_t* owned, int n, int currentIndex, int dir) {
    (void)owned;
    (void)n;
    (void)currentIndex;
    (void)dir;
    return -1;
}
