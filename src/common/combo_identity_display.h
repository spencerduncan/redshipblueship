#pragma once

/**
 * @file combo_identity_display.h
 * @brief What a player reads to tell one paired world from another (#952).
 *
 * THE GAP. The five seed icons at the file select are OoT's: they slice
 * Hash(seed string + OoT's settings string) (3drando/playthrough.cpp's
 * finalHash, spoiler_log.cpp's GenerateHash). The MM options, the MM tricks and
 * the Cross-Game Rules never reach them, so two players with the same seed
 * string and OoT settings see the same icons and get different paired worlds.
 * The pair's own identifier, comboSettingsHash, folds all three halves (the
 * combo rule record, OoT's settings digest and MM's profile digest; context.h)
 * but was printed only in the Combo menu's frozen note and the console-only
 * Combo settings pane, after a file existed.
 *
 * WHAT THIS DRAWS, and only draws: no identity term is computed or changed
 * here, so no generated world and no golden moves.
 *   - Beside the five icons (OoT's file select, z_file_choose.c's
 *     DrawSeedHashSprites, through OoT_FileSelect_DrawComboIdentity in
 *     soh/Enhancements/FileSelectEnhancements.cpp), for a generated paired world
 *     and for a paired file in a slot: the fingerprint as 8 hex digits over the
 *     build's short commit (the one the window title carries, #880). The icons
 *     with the fingerprint cover seed string, OoT settings, MM options and
 *     tricks, and Cross-Game Rules; the commit covers the generator.
 *   - Wherever a pane prints the paired seed: the seed STRING the player typed
 *     (or the ten digits a blank field generated, 3drando/menu.cpp), which can
 *     be typed back in, instead of its hash.
 *
 * Game-header-free (ADR 0002): the seed string lives in OoT's Rando::Context,
 * which OoT hands over through a registered provider. Locked by
 * ComboIdentityDisplay (ROM-free) and drawn by IntPairedFileSelectIdentity.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Room for the file select's two lines: 8 hex digits, a newline, the build. */
#define RSBS_COMBO_IDENTITY_TEXT_CAP 64

/**
 * The build as the file select prints it: the short commit RsbsVersion_Display()
 * carries after the release ("ece5655", "ece5655-dirty"), or the release itself
 * (RSBS_VERSION_STRING) for a build configured without git.
 */
const char* Combo_IdentityDisplay_Build(void);

/**
 * The text drawn beside the five icons: "<fingerprint>\n<build>", the
 * fingerprint as 8 uppercase hex digits. Writes "" and returns 0 for 0 (no
 * frozen paired world: zero is comboSettingsHash's "unset").
 */
int Combo_IdentityDisplay_Format(uint32_t comboSettingsHash, char* out, size_t cap);

/**
 * A generated world's text (name entry, the randomizer menu): gComboCtx's
 * frozen fingerprint while a paired identity is live and belongs to the world
 * OoT has staged, whose seed is @p stagedSeed (Rando::Context::GetSeed()). An
 * identity left from an earlier generation names another world: "" and 0.
 */
int Combo_IdentityDisplay_GeneratedText(uint32_t stagedSeed, char* out, size_t cap);

/**
 * A file's text: the fingerprint its .redsave record froze at creation
 * (rsbs::SlotMeta::comboSettingsHash), "" and 0 for a slot with no record, a
 * refused record or a legacy pair whose rules never froze. Reads every slot
 * under the writer's lock once, then again only when SlotStateEpoch moves (the
 * Save Files page's cache rule), never per frame.
 */
int Combo_IdentityDisplay_SlotText(int slot, char* out, size_t cap);

/** How many times the slot cache has read the disk: the lock's "not per frame". */
int Combo_IdentityDisplay_SlotReadCountForTest(void);

/**
 * Where the paired seed STRING comes from: OoT's Rando::Context, which knows the
 * string it generated or loaded. Writes the string and returns 1 only when its
 * hash is @p sharedRandoSeed (the same SohUtils::Hash generation seeds from),
 * so a string left from another world is never shown as this one's.
 */
typedef int (*ComboSeedStringProvider)(uint32_t sharedRandoSeed, char* out, size_t cap);
/** Returns the provider it replaced, so a lock can put the real one back. */
ComboSeedStringProvider Combo_IdentityDisplay_SetSeedStringProvider(ComboSeedStringProvider provider);

/**
 * The paired seed as a pane prints it: the seed string when the provider has
 * the one this world was generated from, else the number (a pane must still
 * name the world when OoT's context holds no string for it).
 */
void Combo_IdentityDisplay_PairedSeed(uint32_t sharedRandoSeed, char* out, size_t cap);

/** The file select's last drawn text, recorded by the draw: the capture row's
 *  evidence that the text reached the screen, not only the model. */
void Combo_IdentityDisplay_NoteDrawn(const char* text);
const char* Combo_IdentityDisplay_LastDrawn(void);
int Combo_IdentityDisplay_DrawCount(void);

#ifdef __cplusplus
}
#endif
