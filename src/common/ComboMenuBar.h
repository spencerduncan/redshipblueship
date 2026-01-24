#pragma once

#include <libultraship/libultraship.h>
#include <ship/window/gui/GuiMenuBar.h>
#include <ship/window/gui/GuiElement.h>
#include <string>

/**
 * ComboMenuBar - Unified menu bar for the RedShip single executable
 *
 * This menu bar provides:
 * - Shared settings (gCore.*) for Graphics, Audio, Controls, Window
 * - OoT-specific settings (gOoT.*) for Enhancements, Cheats, Cosmetics, Rando
 * - MM-specific settings (gMM.*) for Enhancements, Cheats, Cosmetics, Rando
 * - Cross-game transition settings
 *
 * CVar Namespace Structure:
 *   gCore.Graphics.*  - Shared graphics settings
 *   gCore.Audio.*     - Shared audio settings
 *   gCore.Controls.*  - Shared control settings
 *   gCore.Window.*    - Shared window settings
 *   gOoT.*           - OoT-specific settings
 *   gMM.*            - MM-specific settings
 */

namespace ComboGui {

class ComboMenuBar : public Ship::GuiMenuBar {
public:
    using Ship::GuiMenuBar::GuiMenuBar;

protected:
    void DrawElement() override;
    void InitElement() override;
    void UpdateElement() override {};

private:
    // Main menu drawing functions
    void DrawRedShipMenu();

    // Core settings (shared between games)
    void DrawCoreSettings();
    void DrawGraphicsSettings();
    void DrawAudioSettings();
    void DrawControlSettings();
    void DrawWindowSettings();

    // Game-specific enhancement tabs
    void DrawGameTabs();
    void DrawOoTEnhancements();
    void DrawMMEnhancements();

    // Cross-game transition settings
    void DrawComboSettings();
};

} // namespace ComboGui
