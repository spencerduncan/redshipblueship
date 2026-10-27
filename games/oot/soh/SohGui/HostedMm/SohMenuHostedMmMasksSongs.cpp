/**
 * @file SohMenuHostedMmMasksSongs.cpp
 * @brief Majora's Mask's Masks and Ocarina rows, hosted on SoH's Enhancements > Items page through the hosted-row seam
 *        (#1236, #1240, lane W6-MASKS-SONGS).
 *
 * 2Ship draws these rows on its own Items/Songs page (the excluded BenMenu.cpp), so in the single exe nothing wrote
 * their keys, and every provider but Pause Owl Warp's was left out of the link (games/mm/CMakeLists.txt carves them
 * into ship__Enh_registrars beside this change). Each row binds its RSBS_CVAR_MM_* define
 * (src/common/cvar_shared_keys.h) and SoH's page calls each function below in one line:
 *  - Items > Ocarina: a "Majora's Mask" block after SoH's ocarina rows (Pause Owl Warp, Enable Sun's Song, Zora Eggs
 * for Bossa Nova, Faster Song Playback, Skip Soaring Cutscene, Underwater Ocarina). Underwater Ocarina is a Player key
 * in 2Ship; its row belongs with the Ocarina. SoH's Fast Ocarina Playback is Faster Song Playback's nearest relative,
 *    not its twin: SoH skips the replay of the notes, MM cuts the wait after the song and the Elegy statues' fade.
 *  - Items > Masks: a "Majora's Mask" block after SoH's mask rows (the eight 2Ship Masks options). None has an SoH
 * twin.
 *
 * CROSS-GAME MASKS (#975). None of the mask rows reads where a mask came from. A shared mask found in Hyrule is
 * credited to MM's own inventory slot through the pool, so Equippable While Swimming, Fast Transformation, Fierce
 * Deity's Mask Anywhere and Persistent Bunny Hood see the same item and action ids they see for one found in Termina.
 * An Ocarina of Time mask MM hosts (Rando/ForeignMaskMM.cpp) is worn through its shared slot's C button, which is an MM
 * mask's item action, so Equippable While Swimming lets it on in water, and MM's rules still take it off in another
 * form, Fast Transformation included (ForeignMaskMM applies them every player update). Persistent Bunny Hood keys only
 * on the Bunny Hood, which no foreign variant shares a slot with. Enable Sun's Song makes the song playable without
 * its quest bit, which is not one of the shared songs (Epona's Song, Song of Storms), and plays on the same ocarina
 * buttons the shared-button rule already gates.
 *
 * Every row keeps 2Ship's wording, in SoH's Title Case, and its tooltip ends "Majora's Mask only."
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

namespace SohGui {

using namespace UIWidgets;

/** Items > Ocarina, at the end of the section. */
void AddMmOcarinaRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Pause Owl Warp", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_SONGS_PAUSE_OWL_WARP)
        .Options(CheckboxOptions().Tooltip(
            "Allows warping to activated Owl Statues from the pause menu's map screen, when you can play the Song of "
            "Soaring there. With index warping, every valid warp of the activated points is offered, and Great Bay "
            "Coast is always offered for the first index. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Enable Sun's Song", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_SONGS_ENABLE_SUNS_SONG)
        .Options(CheckboxOptions().Tooltip(
            "Enables the partially implemented Sun's Song: Right, Down, Up, Right, Down, Up. It makes time move very "
            "fast until Link enters another area or the next part of the day begins. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Zora Eggs for Bossa Nova: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_SONGS_ZORA_EGG_COUNT)
        .Options(IntSliderOptions().Min(1).Max(7).DefaultValue(7).Format("%d").Tooltip(
            "Sets how many Zora Eggs unlock the New Wave Bossa Nova. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Faster Song Playback", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_SONGS_FASTER_SONG_PLAYBACK)
        .Options(CheckboxOptions().Tooltip(
            "Speeds up the playback of songs, the Elegy of Emptiness statues included. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Skip Soaring Cutscene", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_SONGS_SKIP_SOARING_CUTSCENE)
        .Options(
            CheckboxOptions().Tooltip("Skips the cutscene when the Song of Soaring warps Link. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Underwater Ocarina", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_UNDERWATER_OCARINA)
        .Options(CheckboxOptions().Tooltip("Allows Zora Link to play the Ocarina of Time while standing on the floor "
                                           "underwater. Majora's Mask only."));
}

/** Items > Masks, at the end of the section. */
void AddMmMasksRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Equippable While Swimming", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_EQUIP_WHILE_SWIMMING)
        .Options(CheckboxOptions().Tooltip(
            "Lets Link put on any mask but a transformation mask while swimming. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Blast Mask Has Powder Keg Force", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_BLAST_MASK_KEG)
        .Options(CheckboxOptions().Tooltip(
            "Lets the Blast Mask also destroy what only a Powder Keg can. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fast Transformation", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_FAST_TRANSFORMATION)
        .Options(
            CheckboxOptions().Tooltip("Removes the delay when Link uses a transformation mask. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fierce Deity's Mask Anywhere", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_FIERCE_DEITYS_ANYWHERE)
        .Options(CheckboxOptions().Tooltip(
            "Allows using the Fierce Deity's Mask outside of boss rooms, in water too, and lets its sword beams hit "
            "enemies anywhere. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Persistent Bunny Hood", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_PERSISTENT_BUNNY_HOOD)
        .Options(CheckboxOptions().Tooltip(
            "Makes A on the Bunny Hood in the mask menu turn its speed boost on or off for good, whatever Link "
            "wears. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "No Blast Mask Cooldown", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_NO_BLAST_MASK_COOLDOWN)
        .Options(CheckboxOptions().Tooltip("Removes the cooldown between Blast Mask explosions. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Goron Rolling Ignores Magic", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_GORON_ROLLING_IGNORES_MAGIC)
        .Options(CheckboxOptions().Tooltip(
            "Lets Goron Link roll with spikes without magic, and spends none. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Goron Rolling Fast Spikes", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_MASKS_GORON_ROLLING_FAST_SPIKES)
        .Options(CheckboxOptions().Tooltip(
            "Speeds up the wind-up to a spiked Goron roll to near instant. Majora's Mask only."));
}

} // namespace SohGui
