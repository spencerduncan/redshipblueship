#pragma once
/**
 * @file OoTMasksMM.h
 * @brief Ocarina of Time's Gerudo, Skull and Spooky Masks as Majora's Mask items
 *        (#980, epic #975 P5; OoTMM gerudoMaskMm, skullMaskMm, spookyMaskMm and
 *        sharedMaskGerudo, sharedMaskSkull, sharedMaskSpooky). The definitions and
 *        the reasoning are in OoTMasksMM.cpp; this header is what the item
 *        pipeline (GiveItem, ConvertItem, GeneratePools, DrawItem, Items.cpp), the
 *        logic (Logic.h) and the locks call. C-compatible.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Player;

/** The worn-mask id (COMBO_FOREIGN_MASK_OOT_GERUDO ... _SPOOKY) of MM item
 *  `riId` (RI_MASK_GERUDO ... RI_MASK_SPOOKY); COMBO_FOREIGN_MASK_NONE for any
 *  other id. Pure. */
uint8_t MM_OoTMask_ForeignId(uint16_t riId);

/** The MM item of worn-mask id `foreignMask`; 0 (RI_UNKNOWN) for any id MM does
 *  not host. Pure. */
uint16_t MM_OoTMask_ItemId(uint8_t foreignMask);

/** Is the frozen extension rule that adds `riId` to MM's pool on
 *  (RSBS_XITEM_GERUDO_MASK_MM ...)? False for any other id and while nothing is
 *  frozen. */
bool MM_OoTMask_RuleOn(uint16_t riId);

/** Does MM's live save hold `riId` (its owned bit in its shared slot's state
 *  byte, foreign_masks.h)? The bit alone, whatever the rules say. */
bool MM_OoTMask_Owned(uint16_t riId);

/** Give `riId` to MM's live save (set its owned bit). No-op for any other id. */
void MM_OoTMask_Give(uint16_t riId);

/** OoTMM's has_mask_gerudo / has_mask_skull / has_mask_spooky in MM: the frozen
 *  rule holds the extension and the live save owns the mask. What MM's logic
 *  reads (Logic.h). */
bool MM_OoTMask_Usable(uint16_t riId);

/**
 * WHAT A LISTED ACTOR SEES (OoTMM's runtime, generator src/mm/actors): with
 * `worn` the mask Link wears through the foreign field (sanitized, human form
 * already checked by the caller), actor `actorId` reads Link as wearing native
 * mask `*maskOut` (PLAYER_MASK_STONE or PLAYER_MASK_GIBDO) or as being in form
 * `*formOut` (PLAYER_FORM_DEKU). 1 when one of the two applies, else 0 (both
 * outputs 0xFF). Pure:
 *  - the Gerudo Mask: the purple guards (EnGe2) read the Stone Mask;
 *  - the Skull Mask: the Deku Palace garden guards (EnLookNuts), Keese
 *    (EnFirefly), Guays (EnCrow) and Bad Bats (EnBat) read the Stone Mask, and
 *    the palace's entrance and throne-room guards (EnGuardNuts, EnDns) read a
 *    Deku;
 *  - the Spooky Mask, at night only (`isNight`): the Gibdos (EnRailgibud,
 *    EnTalkGibud, EnRd) and the Poe collector (EnGb2) read the Gibdo Mask.
 */
int MM_OoTMasks_SpoofFor(uint8_t worn, int16_t actorId, int isNight, uint8_t* maskOut, uint8_t* formOut);

/**
 * Before listed actor `actorId`'s update (its ShouldActorUpdate, `updates` the
 * vote so far): put back anything still held, then, when Link (`player`) is
 * human with no native mask on and wears an OoT mask the actor reacts to
 * (MM_OoTMasks_SpoofFor), set `player`'s currentMask or transformation to what
 * the actor reads until MM_OoTMasks_PutBack. 1 when it set one.
 */
int MM_OoTMasks_BeforeActorUpdate(struct Player* player, int16_t actorId, int updates, int isNight);

/** Put back what MM_OoTMasks_BeforeActorUpdate set on `player` (its update ran,
 *  or another handler kept it from running), and let go. A held value for any
 *  other player (a play state that ended) is forgotten without being written. */
void MM_OoTMasks_PutBack(struct Player* player);

/** Forget a held value without writing it (a play state ending). */
void MM_OoTMasks_Forget(void);

/** Is a value held (a test reads it)? */
int MM_OoTMasks_Holding(void);

/**
 * Register MM's ownership source for the three masks
 * (Combo_ForeignMask_SetOwnedSource) and MM's copies of the three shared
 * families (RSBS_XITEM_SHARED_GERUDO ...). Idempotent: run at start-up and at
 * every file load; the locks call it after rows that clear either registry.
 */
void MM_OoTMasks_RegisterRuntime(void);

/** The pause and tracker icon of `riId`: the OoT mask's own icon in the curated
 *  half when mounted, else its shared slot's own MM icon. NULL for any other id. */
const char* MM_OoTMask_IconPath(uint16_t riId);

/** Draw `riId`'s get-item model (DrawItem.cpp): Ocarina of Time's own model from
 *  the co-mounted archives, else its shared slot's own MM model. */
void MM_OoTMask_DrawGetItem(uint16_t riId);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
