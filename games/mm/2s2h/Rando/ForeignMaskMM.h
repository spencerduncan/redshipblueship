#pragma once
/**
 * @file ForeignMaskMM.h
 * @brief MM wears and draws an Ocarina of Time mask (#977, epic #975 P3). The
 *        definitions and the reasoning are in ForeignMaskMM.cpp; this header is
 *        what the mask page (z_kaleido_mask.c), the locks and the items P5 adds
 *        call. C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Player;

/** The worn OoT mask: the stored ComboForeignMask when MM wears it and the
 *  frozen world added it (Combo_ForeignMask_Wearable), else NONE. */
uint8_t MM_ForeignMask_Worn(void);

/** Put `mask` on (or take it off when worn): human form only, and only a mask
 *  the frozen world added. Putting one on takes the native mask off. 1 when the
 *  worn field changed, else 0. P5's items call this. */
int MM_ForeignMask_Use(struct Player* player, uint8_t mask);

/** MM's mask rules on the worn field, once per player update: a transformation,
 *  a native mask put on, or a rule that is off takes it off. */
void MM_ForeignMask_ApplyRules(struct Player* player);

/** A shared pause slot's chosen variant: an owned OoT mask sharing it, or NONE
 *  (the slot's own mask). */
uint8_t MM_ForeignMask_SlotSelection(uint8_t slot);

/** Cycle a shared slot's variant (dir > 0 forward). 1 when it changed. */
int MM_ForeignMask_CycleSlot(uint8_t slot, int dir);

/** How many variants a shared slot cycles through (below 2: no cycling). */
int MM_ForeignMask_SlotVariantCount(uint8_t slot);

/** VB_USE_ITEM_EQUIP_MASK: native mask `maskId`'s C button was used. 1 when its
 *  slot's chosen foreign variant was used instead (the vanilla equip must not
 *  run), else 0. */
int MM_ForeignMask_OnEquipNative(struct Player* player, int maskId);

/** The mask page's icon for cell `maskSlot` (0-based), whose own item's icon is
 *  `nativeIcon`: the chosen variant's icon on a shared slot when
 *  redship-mm.o2r holds it, else `nativeIcon`. */
const char* MM_ForeignMask_PageIcon(int maskSlot, const char* nativeIcon);

/** The list MM's head draw would emit for the live worn field, or NULL. */
const char* MM_ForeignMask_WornDl(struct Player* player);

/** The fields' raw bytes and the worn field's offset in SaveContext (0: no
 *  field). The locks read and write the fields through these, never by name. */
uint8_t MM_ForeignMask_StoredRaw(void);
void MM_ForeignMask_SetStoredRaw(uint8_t value);
uint8_t MM_ForeignMask_SlotSelRaw(uint8_t slot);
void MM_ForeignMask_SetSlotSelRaw(uint8_t slot, uint8_t value);
size_t MM_ForeignMask_FieldOffset(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
