/**
 * @file SohMenuHostedMmTimesavers.cpp
 * @brief Majora's Mask text, time-saver and saving rows hosted on SoH's Enhancements pages (#1236, #1240), through the
 *        hosted-row seam (SohMenuHostedMm.h).
 *
 * MM read every key below, but its only widgets sat in the excluded BenMenu.cpp, so nothing in the single exe wrote
 * them, and the eleven TUs behind the hooked ones were not in the link at all, nor was Fixes/BgmReplay.cpp, which keeps
 * MM's main BGM player valid under two of them (games/mm/CMakeLists.txt, W6-TIMESAVERS).
 * SoH's pages call these functions in one line each (SohMenuEnhancements.cpp):
 *  - Skips & Speed-ups > Text: MM Fast Text right after SoH's Text Speed, its twin. The pair stays apart (P1 in
 *    kMustStayDistinct: SoH's 1-5x multiplier against MM's on/off, which also bundles hold-B-to-advance), so the row
 *    is MM's own. Fast Bank Selection and Auto Bombers' Code, MM's other dialogue rows, close the section.
 *  - Skips & Speed-ups > Misc: a "Majora's Mask" block at the end of the column for the eight time savers, which have
 *    no SoH twin.
 *  - Quality of Life > Saving: Pause Menu Save, after SoH's own saving rows. Its save reaches disk through the paired
 *    .redsave commit (func_8014546C -> Sram_ComboCommitUnifiedSave), not MM's flash.
 * MM's other saving options get no row, because none can act in the paired game. A paired MM session runs at the 0xFF
 * fileNum with no flash slot: Persistent Owl Saves gates only the owl save's flash delete on continue
 * (func_80147314 returns at once), and Disable Save Delay only the timer of a flash write that is never started. 3rd
 * Save File Slot shapes MM's file select, which paired play never shows (the file select is OoT's).
 * docs/enhancement-classification.md records the three.
 * Every row binds its RSBS_CVAR_MM_* define (cvar_shared_keys.h, W6-TIMESAVERS slot) and says "Majora's Mask only."
 */

#include "SohMenuHostedMm.h"

#include "soh/SohGui/UIWidgets.hpp"

#include "cvar_shared_keys.h" // src/common: the RSBS_CVAR_MM_* keys

namespace SohGui {

using namespace UIWidgets;

void AddMmFastTextRow(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Text", "MM Fast Text", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DIALOGUE_FAST_TEXT)
        .Options(CheckboxOptions().Tooltip(
            "Speeds up text rendering, and holding B advances to the next message. Majora's Mask only."));
}

void AddMmDialogueRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Text", "Fast Bank Selection", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DIALOGUE_FAST_BANK_SELECTION)
        .Options(CheckboxOptions().Tooltip("Sets the amount to all of Link's Rupees with Z, or to 0 with R, while the "
                                           "bank's deposit or withdrawal amount is open. Majora's Mask only."));
    AddHostedMmWidget(menu, path, "Text", "Auto Bombers' Code", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_DIALOGUE_AUTO_BOMBERS_CODE)
        .Options(CheckboxOptions().Tooltip(
            "Fills in the Bombers' secret code once you have the Bombers' Notebook. Majora's Mask only."));
}

void AddMmTimesaverRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmBlock(menu, path);
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Swamp Boat Timesaver", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_SWAMP_BOAT_SPEED)
        .Options(CheckboxOptions().Tooltip("Speeds up the boat ride through the swamp while Z is held. Majora's Mask "
                                           "only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Shooting Gallery Both Rewards", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_GALLERY_TWOFER)
        .Options(CheckboxOptions().Tooltip("Gives both of a Shooting Gallery's rewards for one perfect score, back to "
                                           "back, instead of one per game. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fast Marine Lab Fish", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_MARINE_LAB_HP)
        .Options(
            CheckboxOptions().Tooltip("Spawns the Marine Research Lab's Piece of Heart after a single fish is fed. "
                                      "Takes effect after a scene reload. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Fast Dampe Flame Digging", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_DAMPE_DIGGING_SKIP)
        .Options(CheckboxOptions().Tooltip(
            "Summons the Big Poe after Dampe digs up one flame instead of three. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Faster Scene Transitions", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_FASTER_SCENE_TRANSITIONS)
        .Options(
            CheckboxOptions().Tooltip("Fades in and out more quickly when moving between areas. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Skip Powder Keg Certification", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_POWDER_KEG_CERTIFICATION)
        .Options(CheckboxOptions().Tooltip(
            "Grants the Powder Keg certification without the Powder Keg test. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Skip Ballad of Windfish", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_SKIP_BALLAD_OF_WINDFISH)
        .Options(CheckboxOptions().Tooltip("Counts the whole Ballad of the Windfish after it is played in one form, "
                                           "once all three transformation masks are owned. Majora's Mask only."));
    AddHostedMmWidget(menu, path, kHostedMmBlockLabel, "Auto Bank Deposit", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_TIMESAVERS_AUTO_BANK_DEPOSIT)
        .Options(CheckboxOptions().Tooltip("Deposits the Rupees a full wallet cannot hold into the bank, up to its "
                                           "limit, and grants the bank's rewards on the way. Majora's Mask only."));
}

void AddMmSavingRows(SohMenu& menu, WidgetPath& path) {
    AddHostedMmWidget(menu, path, "Saving", "Pause Menu Save", WIDGET_CVAR_CHECKBOX)
        .CVar(RSBS_CVAR_MM_SAVING_PAUSE_SAVE)
        .Options(CheckboxOptions().Tooltip("Offers a save from the current location when B is pressed in the pause "
                                           "menu. Majora's Mask only."));
}

} // namespace SohGui
