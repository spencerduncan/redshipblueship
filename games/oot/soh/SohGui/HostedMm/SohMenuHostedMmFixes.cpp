/**
 * @file SohMenuHostedMmFixes.cpp
 * @brief Majora's Mask's Fixes and Restorations rows, hosted on SoH's Enhancements > Fixes page through the hosted-row
 *        seam (#1237, #1236, lane W6-FIXES).
 *
 * Fifteen 2Ship rows whose only widgets sat in the excluded BenMenu.cpp, so nothing in the single exe wrote their keys.
 * Eleven of them also had their TU dropped from the link (games/mm/CMakeLists.txt carves them into
 * ship__Enh_registrars beside this change); the other four are read in MM's own C (Fix Targeting Camera Snap in
 * z_player.c, Fix Ammo Count Color in z_parameter.c, Fix Ikana Great Fairy Fountain Color in z_kankyo.c, Fix Text
 * Control Characters in z_message_nes.c) and needed only a row. Each row binds its RSBS_CVAR_MM_* define
 * (src/common/cvar_shared_keys.h), and SoH's page calls each function below in one line, at the end of the section
 * that holds its nearest relatives:
 *  - Gameplay Fixes: Fix Epona Stealing Sword, Fix Fierce Deity Z-Target Movement.
 *  - Item-related Fixes: MM Power Crouch Stab, right after SoH's Remove Power Crouch Stab. It means the opposite (MM's
 *    row RESTORES the glitch SoH's row removes) and its values differ, so the keys are never mapped; and it has its
 *    own visibility, so it does not inherit SoH's row's Fix Megaton Hammer Crouch Stab gate.
 *  - Camera Fixes: Fix Targeting Camera Snap.
 *  - Graphical Fixes: Fix Ammo Count Color, Fix Ikana Great Fairy Fountain Color, Fix Text Control Characters.
 *  - Audio Fixes: Fix Completed Heart Container Audio.
 *  - Glitch Restorations: Tatl ISG (restoration), labelled as ADR 0004 rules: held apart from SoH's Easy ISG (P2 in
 *    kMustStayDistinct), which is a Cheat.
 *  - Misc Restorations: the six behaviour restorations.
 * Fix Console Crashes has no row: the single exe always keeps the crash fixes on (src/common/mm_stubs.c).
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* defines

#include <map>

namespace SohGui {

/** Enhancements > Fixes > Gameplay Fixes, at the end of the section. */
void AddMmGameplayFixesRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Gameplay Fixes", "Fix Epona Stealing Sword", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_FIXES_EPONA_STEALING_SWORD)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Fixes a bug where Epona takes Link's sword when he mounts her "
                                                      "without a Bow in his inventory. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Gameplay Fixes", "Fix Fierce Deity Z-Target Movement", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_FIXES_FIERCE_DEITY_Z_TARGET_MOVEMENT)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Fixes Fierce Deity Link's movement being choppy while Z-targeting. Majora's Mask only."));
}

/** Enhancements > Fixes > Item-related Fixes, right after SoH's Remove Power Crouch Stab, always shown. */
void AddMmPowerCrouchStabRow(SohMenu& menu, WidgetPath& path) {
    // 2Ship's three values, in its order. Static storage: the combobox keeps a pointer to the map.
    static const std::map<int32_t, const char*> mmPowerCrouchStabOptions = {
        { 0, "Patched (US/EU)" },
        { 1, "Unpatched (JP)" },
        { 2, "Unpatched (OoT)" },
    };
    AddHostedMmWidget(menu, path, "Item-related Fixes", "MM Power Crouch Stab", WIDGET_CVAR_COMBOBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_POWER_CROUCH_STAB)
        .Options(UIWidgets::ComboboxOptions()
                     .ComboMap(mmPowerCrouchStabOptions)
                     .DefaultIndex(0)
                     .Tooltip("Sets whether crouch stabs use the power of Link's previous melee attack.\n"
                              "- Patched (US/EU): Crouch stabs always do the same damage as a slash with the current "
                              "weapon.\n"
                              "- Unpatched (JP): The glitch is restored, and the first crouch stab does no damage "
                              "(useful for the Infinite Sword Glitch on pots).\n"
                              "- Unpatched (OoT): The glitch is restored, and the first crouch stab does the damage of "
                              "a Kokiri Sword slash.\n\n"
                              "Majora's Mask only."));
}

/** Enhancements > Fixes > Camera Fixes, at the end of the section. */
void AddMmCameraFixesRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Camera Fixes", "Fix Targeting Camera Snap", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_CAMERA_FIX_TARGETTING_CAMERA_SNAP)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Fixes the camera snap that occurs when Link is moving and the "
                                                      "targeting button is pressed. Majora's Mask only."));
}

/** Enhancements > Fixes > Graphical Fixes, at the end of the section. */
void AddMmGraphicalFixesRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Graphical Fixes", "Fix Ammo Count Color", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_FIXES_AMMO_COUNT_ENV_COLOR)
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Fixes a missing color setting that draws the ammo count in the wrong color before Link has magic, among "
            "other conditions. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Graphical Fixes", "Fix Ikana Great Fairy Fountain Color", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_FIXES_IKANA_GREAT_FAIRY_FOUNTAIN_COLOR)
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Fixes the Ikana Great Fairy Fountain looking green instead of yellow, as the European release did. "
            "Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Graphical Fixes", "Fix Text Control Characters", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_FIXES_CONTROL_CHARACTERS)
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Fixes certain text control characters not working, depending "
                                                      "on where they sit in the text. Majora's Mask only."));
}

/** Enhancements > Fixes > Audio Fixes, at the end of the section. */
void AddMmAudioFixesRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Audio Fixes", "Fix Completed Heart Container Audio", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_FIXES_COMPLETED_HEART_CONTAINER_AUDIO)
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions().Tooltip("Fixes the wrong sound playing when a fourth Piece of Heart "
                                                      "completes a new Heart Container. Majora's Mask only."));
}

/** Enhancements > Fixes > Glitch Restorations, at the end of the section. */
void AddMmGlitchRestorationsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Glitch Restorations", "Tatl ISG (restoration)", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_TATL_ISG)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Restores Navi's Infinite Sword Glitch from Ocarina of Time, performed with Tatl. Majora's Mask only."));
}

/** Enhancements > Fixes > Misc Restorations, at the end of the section. */
void AddMmMiscRestorationsRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Misc Restorations", "Constant Distance Backflips and Sidehops", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_CONSTANT_FLIPS_HOPS)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Makes backflips and sidehops travel a constant distance, as in Ocarina of Time. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Misc Restorations", "Side Rolls", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_SIDE_ROLL)
        .Options(
            UIWidgets::CheckboxOptions().Tooltip("Restores side rolling from Ocarina of Time. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Misc Restorations", "Faster Swim", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_OOT_FASTER_SWIM)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Restores swimming faster by pressing B repeatedly, as in Ocarina of Time. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Misc Restorations", "Woodfall Mountain Appearance", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_WOODFALL_MOUNTAIN_APPEARANCE)
        .RaceDisable(false)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Restores Woodfall's mountain to its unpoisoned look from Termina Field once Woodfall Temple is cleared. "
            "Takes effect on the next scene load. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Misc Restorations", "JP Deku Palace Grottos", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_JP_GROTTOS)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Restores the Deku Palace grottos to their original Japanese layout. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Misc Restorations", "Bonk Collision", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_RESTORATIONS_BONK_COLLISION)
        .Options(UIWidgets::CheckboxOptions().Tooltip(
            "Lets a roll bonk into trees near its end, as in Ocarina of Time. Majora's Mask only."));
}

} // namespace SohGui
