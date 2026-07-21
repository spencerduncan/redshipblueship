#include "Spoiler.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include "BenPort.h"

#include <libultraship/libultra/types.h>

std::vector<std::string> Rando::Spoiler::spoilerOptions;

// Lazy on purpose (#392 Lane C0): as a namespace-scope global this path's
// dynamic initializer called Ship::Context::GetPathRelativeToAppDirectory()
// BEFORE main() once 2ship_rando stopped being link-elided — before OoT
// creates the shared Ship::Context — killing the exe at boot. A
// function-local static defers the call to first use, which is always after
// context bring-up (MM_Rando_Init or the spoiler read/write paths).
static const std::filesystem::path& GetRandomizerFolderPath() {
    static const std::filesystem::path randomizerFolderPath(
        Ship::Context::GetPathRelativeToAppDirectory("randomizer", appShortName));
    return randomizerFolderPath;
}

// This function refreshes the list of spoiler files in the randomizer folder, this list is used in the Randomizer UI,
// and also includes an option to generate a new seed at the top of the list.
void Rando::Spoiler::RefreshOptions() {
    Rando::Spoiler::spoilerOptions.clear();

    Rando::Spoiler::spoilerOptions.push_back("Generate New Seed");
    s32 spoilerFileIndex = -1;

    // ensure the randomizer folder exists
    if (!std::filesystem::exists(GetRandomizerFolderPath())) {
        std::filesystem::create_directory(GetRandomizerFolderPath());
    }

    // Add all files in the randomizer folder to the list of spoiler options
    for (const auto& entry : std::filesystem::directory_iterator(GetRandomizerFolderPath())) {
        if (entry.is_regular_file()) {
            std::string fileName = entry.path().filename().string();
            Rando::Spoiler::spoilerOptions.push_back(fileName);

            // Check if the current file is the one set in the cvar
            if (fileName == CVarGetString("gRando.SpoilerFile", "")) {
                spoilerFileIndex = Rando::Spoiler::spoilerOptions.size() - 1;
            }
        }
    }

    // If the current spoiler file is not in the randomizer folder, reset the cvar
    if (spoilerFileIndex == -1) {
        CVarSetInteger("gRando.SpoilerFileIndex", 0);
        CVarSetString("gRando.SpoilerFile", "");
    } else {
        CVarSetInteger("gRando.SpoilerFileIndex", spoilerFileIndex);
    }
}
