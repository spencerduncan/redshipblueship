/**
 * @file SohMenuHostedMmPlayer.cpp
 * @brief Majora's Mask's Player, Equipment and PlayerActions rows, hosted on SoH's Enhancements pages through the
 *        hosted-row seam (#1236, lane W6-PLAYER).
 *
 * Twelve 2Ship rows whose only widgets sat in the excluded BenMenu.cpp, so nothing in the single exe wrote their keys
 * and none of their TUs was linked (games/mm/CMakeLists.txt carves them into ship__Enh_registrars beside this change).
 * Each row binds its RSBS_CVAR_MM_* define (src/common/cvar_shared_keys.h), and SoH's page calls each function below
 * in one line:
 *  - Quality of Life > Controls: a "Majora's Mask" block (Manual Jump, Prevent Diving Over Water, Infinite Deku
 *    Hopping). SoH has no twin of any of them.
 *  - Skips & Speed-ups > Animations: "MM Vine/Ladder Climb Speed" right after SoH's own. The pair is held apart
 *    (kMustStayDistinct): SoH adds an offset (+0-12), MM multiplies (1-5x). Faster Push/Pull right after SoH's Block
 *    Pushing Speed, its nearest relative (one switch over every pushed and pulled object, not a speed step). A
 *    "Majora's Mask" block at the end of the section for Fast Deku Flower Launch and Fierce Deity Putaway.
 *  - Items > Equipment: a "Majora's Mask" block (Two-Handed Sword Spin Attack, Great Fairy Sword B-Button Attack,
 *    Better Picto Message).
 *  - Items > Boomerang: Instant Fin Boomerangs Recall right after SoH's Instant Boomerang Recall, its nearest relative
 *    (Zora Link's fins and the B button, not the Boomerang and its item button).
 *  - Difficulty > Drops: "MM Enable Bombchu Drops" right after SoH's own. Held apart (kMustStayDistinct): SoH's rule
 *    weighs what Link carries and an Ocarina of Time seed can force it on; MM's is an even chance on every Bomb drop.
 * Fierce Deity Putaway and Great Fairy Sword B-Button Attack touch only how the held sword is put away or swung; no
 * mask the combo owns is read or written.
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

namespace SohGui {

/** Quality of Life > Controls, at the end of the section. */
void AddMmControlsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Manual Jump", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_MANUAL_JUMP)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Makes Z + A jump instead of jump attacking; press B in midair to jump attack. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Prevent Diving Over Water", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_PREVENT_DIVE_OVER_WATER)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Prevents Link from automatically diving over bodies of water. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Infinite Deku Hopping", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_INFINITE_DEKU_HOPPING)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Lets Deku Link hop across water without limit and without "
                                                      "losing speed in the air. Majora's Mask only."));
}

/** Skips & Speed-ups > Animations, right after SoH's Vine/Ladder Climb Speed. */
void AddMmClimbSpeedRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Animations", "MM Vine/Ladder Climb Speed %dx", WIDGET_CVAR_SLIDER_INT)
        .CVar(RSBS_CVAR_MM_PLAYER_CLIMB_SPEED)
        .Options(UIWidgets::IntSliderOptions().Min(1).Max(5).DefaultValue(1).Format("%dx").Tooltip(
            "Multiplies how fast Link climbs vines and ladders. Majora's Mask only."));
}

/** Skips & Speed-ups > Animations, right after SoH's Block Pushing Speed. */
void AddMmPushPullRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Animations", "Faster Push/Pull", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FASTER_PUSH_AND_PULL)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Speeds up pushing and pulling blocks and other objects. Majora's Mask only."));
}

/** Skips & Speed-ups > Animations, at the end of the section. */
void AddMmAnimationsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fast Deku Flower Launch", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FAST_FLOWER_LAUNCH)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Shortens the wait before a Deku Flower launch reaches its full height. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fierce Deity Putaway", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_FIERCE_DEITY_PUTAWAY)
        .Options(
            UIWidgets::CheckboxOptions().Tooltip("Lets Fierce Deity Link put away his sword. Majora's Mask only."));
}

/** Items > Equipment, at the end of the section. */
void AddMmEquipmentRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Two-Handed Sword Spin Attack", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_TWO_HANDED_SWORD_SPIN_ATTACK)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Enables magic spin attacks with the Fierce Deity's Sword and "
                                                      "the Great Fairy's Sword. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Great Fairy Sword B-Button Attack", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_GREAT_FAIRY_SWORD_B_BUTTON)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Makes B attack with the Great Fairy's Sword while it is held, instead of drawing your equipped sword. "
            "A still puts it away. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Better Picto Message", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_BETTER_PICTO_MESSAGE)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Names the target, if any, that a pictograph has captured. Majora's Mask only."));
}

/** Items > Boomerang, right after SoH's Instant Boomerang Recall. */
void AddMmFinBoomerangRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Boomerang", "Instant Fin Boomerangs Recall", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_PLAYER_ACTIONS_INSTANT_RECALL)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Recalls Zora Link's fin boomerangs at once when B is pressed after they are thrown. Majora's Mask only."));
}

/** Difficulty > Drops, right after SoH's Enable Bombchu Drops. */
void AddMmBombchuDropsRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Drops", "MM Enable Bombchu Drops", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_EQUIPMENT_CHU_DROPS)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Gives every Bomb drop an even chance to be a Bombchu instead, "
                                                      "whatever Link carries. Majora's Mask only."));
}

} // namespace SohGui
