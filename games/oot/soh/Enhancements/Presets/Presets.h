#pragma once

#include <string>
#include <vector>

enum PresetSection {
    PRESET_SECTION_SETTINGS,
    PRESET_SECTION_ENHANCEMENTS,
    PRESET_SECTION_AUDIO,
    PRESET_SECTION_COSMETICS,
    PRESET_SECTION_RANDOMIZER,
    PRESET_SECTION_TRACKERS,
    PRESET_SECTION_NETWORK,
    PRESET_SECTION_MAX,
};

void DrawPresetSelector(std::vector<PresetSection> includeSections, std::string currentIndex, bool disabled);
void applyPreset(std::string presetName, std::vector<PresetSection> includeSections = {});

#ifdef RSBS_SINGLE_EXECUTABLE
// Test seam (#1210): runs LoadPresets()'s pass over the user presets folder with `folder` standing in for the app's
// presets/ folder, and returns the names of the presets it listed. The archive built-ins are not read (no resource
// manager is needed), and the folder setting and the loaded preset list are put back before it returns.
std::vector<std::string> Presets_LoadFolderForTest(const std::string& folder);
#endif
