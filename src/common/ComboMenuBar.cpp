#include "ComboMenuBar.h"

#include <imgui.h>
#include <string>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <libultraship/bridge/consolevariablebridge.h>

namespace ComboGui {

// CVar namespace prefixes for the unified settings system
namespace CVarPrefix {
    // Core (shared) settings
    constexpr const char* CORE_GRAPHICS = "gCore.Graphics";
    constexpr const char* CORE_AUDIO = "gCore.Audio";
    constexpr const char* CORE_CONTROLS = "gCore.Controls";
    constexpr const char* CORE_WINDOW = "gCore.Window";

    // OoT-specific settings
    constexpr const char* OOT_ENHANCEMENTS = "gOoT.Enhancements";
    constexpr const char* OOT_CHEATS = "gOoT.Cheats";
    constexpr const char* OOT_COSMETICS = "gOoT.Cosmetics";
    constexpr const char* OOT_RANDO = "gOoT.Rando";

    // MM-specific settings
    constexpr const char* MM_ENHANCEMENTS = "gMM.Enhancements";
    constexpr const char* MM_CHEATS = "gMM.Cheats";
    constexpr const char* MM_COSMETICS = "gMM.Cosmetics";
    constexpr const char* MM_RANDO = "gMM.Rando";

    // Cross-game settings
    constexpr const char* COMBO = "gCore.Combo";
}

// Helper to create a full CVar name
static std::string MakeCVar(const char* prefix, const char* name) {
    return std::string(prefix) + "." + name;
}

void ComboMenuBar::InitElement() {
    // Initialization - can be used to set up default values
}

void ComboMenuBar::DrawElement() {
    if (ImGui::BeginMenuBar()) {
        static ImVec2 sWindowPadding(8.0f, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, sWindowPadding);

        ImGui::SetCursorPosY(0.0f);

        // RedShip main menu (app-level controls)
        DrawRedShipMenu();

        ImGui::SetCursorPosY(0.0f);

        // Core settings menu (shared settings)
        DrawCoreSettings();

        ImGui::SetCursorPosY(0.0f);

        // Game-specific tabs
        DrawGameTabs();

        ImGui::SetCursorPosY(0.0f);

        // Cross-game combo settings
        DrawComboSettings();

        ImGui::PopStyleVar(1);
        ImGui::EndMenuBar();
    }
}

void ComboMenuBar::DrawRedShipMenu() {
    if (ImGui::BeginMenu("RedShip")) {
        if (ImGui::MenuItem("Hide Menu Bar",
#if !defined(__SWITCH__) && !defined(__WIIU__)
                            "F1"
#else
                            "[-]"
#endif
                            )) {
            Ship::Context::GetInstance()->GetWindow()->GetGui()->GetMenuBar()->ToggleVisibility();
        }

#if !defined(__SWITCH__) && !defined(__WIIU__)
        if (ImGui::MenuItem("Toggle Fullscreen", "F11")) {
            Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen();
        }
#endif

        if (ImGui::MenuItem("Reset",
#ifdef __APPLE__
                            "Command-R"
#elif !defined(__SWITCH__) && !defined(__WIIU__)
                            "Ctrl+R"
#else
                            ""
#endif
                            )) {
            std::reinterpret_pointer_cast<Ship::ConsoleWindow>(
                Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Console"))
                ->Dispatch("reset");
        }

#if !defined(__SWITCH__) && !defined(__WIIU__)
        ImGui::Separator();

        if (ImGui::MenuItem("Quit")) {
            Ship::Context::GetInstance()->GetWindow()->Close();
        }
#endif

        ImGui::EndMenu();
    }
}

void ComboMenuBar::DrawCoreSettings() {
    if (ImGui::BeginMenu("Settings")) {
        // Graphics submenu
        if (ImGui::BeginMenu("Graphics")) {
            DrawGraphicsSettings();
            ImGui::EndMenu();
        }

        // Audio submenu
        if (ImGui::BeginMenu("Audio")) {
            DrawAudioSettings();
            ImGui::EndMenu();
        }

        // Controls submenu
        if (ImGui::BeginMenu("Controls")) {
            DrawControlSettings();
            ImGui::EndMenu();
        }

        // Window submenu
        if (ImGui::BeginMenu("Window")) {
            DrawWindowSettings();
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void ComboMenuBar::DrawGraphicsSettings() {
    // Placeholder for graphics settings
    // TODO: Add actual graphics settings with gCore.Graphics.* CVars
    ImGui::Text("Graphics Settings");
    ImGui::Separator();

    ImGui::TextDisabled("(Settings will be populated here)");

    // Example placeholder CVars (to be implemented):
    // - gCore.Graphics.InternalResolution
    // - gCore.Graphics.MSAA
    // - gCore.Graphics.TextureFilter
    // - gCore.Graphics.Framerate
    // - gCore.Graphics.VSync
}

void ComboMenuBar::DrawAudioSettings() {
    // Placeholder for audio settings
    // TODO: Add actual audio settings with gCore.Audio.* CVars
    ImGui::Text("Audio Settings");
    ImGui::Separator();

    ImGui::TextDisabled("(Settings will be populated here)");

    // Example placeholder CVars (to be implemented):
    // - gCore.Audio.MasterVolume
    // - gCore.Audio.MusicVolume
    // - gCore.Audio.SFXVolume
    // - gCore.Audio.Backend
}

void ComboMenuBar::DrawControlSettings() {
    // Placeholder for control settings
    // TODO: Add actual control settings with gCore.Controls.* CVars
    ImGui::Text("Control Settings");
    ImGui::Separator();

    ImGui::TextDisabled("(Settings will be populated here)");

    // Example placeholder CVars (to be implemented):
    // - gCore.Controls.RumbleStrength
    // - gCore.Controls.GyroSensitivity
    // - gCore.Controls.Deadzone
}

void ComboMenuBar::DrawWindowSettings() {
    // Placeholder for window settings
    // TODO: Add actual window settings with gCore.Window.* CVars
    ImGui::Text("Window Settings");
    ImGui::Separator();

    ImGui::TextDisabled("(Settings will be populated here)");

    // Example placeholder CVars (to be implemented):
    // - gCore.Window.Width
    // - gCore.Window.Height
    // - gCore.Window.Fullscreen
    // - gCore.Window.Borderless
}

void ComboMenuBar::DrawGameTabs() {
    if (ImGui::BeginMenu("Enhancements")) {
        // Use ImGui tab bar to switch between OoT and MM enhancements
        if (ImGui::BeginTabBar("GameEnhancementTabs", ImGuiTabBarFlags_None)) {
            // OoT Enhancements tab
            if (ImGui::BeginTabItem("Ocarina of Time")) {
                DrawOoTEnhancements();
                ImGui::EndTabItem();
            }

            // MM Enhancements tab
            if (ImGui::BeginTabItem("Majora's Mask")) {
                DrawMMEnhancements();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndMenu();
    }
}

void ComboMenuBar::DrawOoTEnhancements() {
    // Placeholder for OoT-specific enhancements
    // TODO: Add actual OoT enhancements with gOoT.* CVars
    ImGui::Text("Ocarina of Time Enhancements");
    ImGui::Separator();

    ImGui::TextDisabled("(OoT enhancements will be populated here)");

    // Categories to be implemented:
    // - Gameplay (gOoT.Enhancements.*)
    // - Graphics (gOoT.Enhancements.Graphics.*)
    // - Fixes (gOoT.Enhancements.Fixes.*)
    // - Restoration (gOoT.Enhancements.Restoration.*)
    // - Cheats (gOoT.Cheats.*)
    // - Cosmetics (gOoT.Cosmetics.*)
    // - Randomizer (gOoT.Rando.*)

    if (ImGui::CollapsingHeader("Gameplay")) {
        ImGui::TextDisabled("(Gameplay enhancements)");
    }

    if (ImGui::CollapsingHeader("Cheats")) {
        ImGui::TextDisabled("(OoT Cheats)");
    }

    if (ImGui::CollapsingHeader("Cosmetics")) {
        ImGui::TextDisabled("(OoT Cosmetics)");
    }

    if (ImGui::CollapsingHeader("Randomizer")) {
        ImGui::TextDisabled("(OoT Randomizer options)");
    }
}

void ComboMenuBar::DrawMMEnhancements() {
    // Placeholder for MM-specific enhancements
    // TODO: Add actual MM enhancements with gMM.* CVars
    ImGui::Text("Majora's Mask Enhancements");
    ImGui::Separator();

    ImGui::TextDisabled("(MM enhancements will be populated here)");

    // Categories to be implemented:
    // - Gameplay (gMM.Enhancements.*)
    // - Graphics (gMM.Enhancements.Graphics.*)
    // - Fixes (gMM.Enhancements.Fixes.*)
    // - Restoration (gMM.Enhancements.Restoration.*)
    // - Cheats (gMM.Cheats.*)
    // - Cosmetics (gMM.Cosmetics.*)
    // - Randomizer (gMM.Rando.*)

    if (ImGui::CollapsingHeader("Gameplay")) {
        ImGui::TextDisabled("(Gameplay enhancements)");
    }

    if (ImGui::CollapsingHeader("Cheats")) {
        ImGui::TextDisabled("(MM Cheats)");
    }

    if (ImGui::CollapsingHeader("Cosmetics")) {
        ImGui::TextDisabled("(MM Cosmetics)");
    }

    if (ImGui::CollapsingHeader("Randomizer")) {
        ImGui::TextDisabled("(MM Randomizer options)");
    }
}

void ComboMenuBar::DrawComboSettings() {
    if (ImGui::BeginMenu("Combo")) {
        ImGui::Text("Cross-Game Settings");
        ImGui::Separator();

        ImGui::TextDisabled("(Cross-game transition settings)");

        // Placeholder for combo-specific settings
        // TODO: Add actual combo settings with gCore.Combo.* CVars

        // Example placeholder CVars (to be implemented):
        // - gCore.Combo.AutoSwitchOnWarp
        // - gCore.Combo.PreservePosition
        // - gCore.Combo.TransitionEffect
        // - gCore.Combo.SharedInventory

        if (ImGui::CollapsingHeader("Game Switching")) {
            ImGui::TextDisabled("(Game transition behavior)");
            // Settings for how games switch between each other
        }

        if (ImGui::CollapsingHeader("Save Data")) {
            ImGui::TextDisabled("(Cross-game save data options)");
            // Settings for save state preservation across games
        }

        if (ImGui::CollapsingHeader("Visual Transitions")) {
            ImGui::TextDisabled("(Transition effects between games)");
            // Settings for visual effects when switching games
        }

        ImGui::EndMenu();
    }
}

} // namespace ComboGui
