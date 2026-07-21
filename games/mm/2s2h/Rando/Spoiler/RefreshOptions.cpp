#include "Spoiler.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <cstdio>
#include "BenPort.h"

#include <libultraship/libultra/types.h>

std::vector<std::string> Rando::Spoiler::spoilerOptions;

// Lazy on purpose (#392 Lane C0): as a namespace-scope global this path's
// dynamic initializer called Ship::Context::GetPathRelativeToAppDirectory()
// BEFORE main() once 2ship_rando stopped being link-elided — before OoT
// creates the shared Ship::Context — killing the exe at boot. A
// function-local static defers the call to first use, which is always after
// context bring-up (MM_Rando_Init or the spoiler read/write paths).
//
// The path itself now comes from Rando::Spoiler::SpoilerDirectory() (#439) so
// this listing and the read/write paths can never disagree about where MM
// spoilers live — they did, and the write path landed nowhere findable.
static const std::filesystem::path& GetRandomizerFolderPath() {
    static const std::filesystem::path randomizerFolderPath(Rando::Spoiler::SpoilerDirectory());
    return randomizerFolderPath;
}

// This function refreshes the list of spoiler files in the randomizer folder, this list is used in the Randomizer UI,
// and also includes an option to generate a new seed at the top of the list.
void Rando::Spoiler::RefreshOptions() {
    Rando::Spoiler::spoilerOptions.clear();

    Rando::Spoiler::spoilerOptions.push_back("Generate New Seed");
    s32 spoilerFileIndex = -1;

    // ensure the randomizer folder exists (SpoilerDirectory already creates it
    // recursively; this covers a folder deleted after first use)
    if (!std::filesystem::exists(GetRandomizerFolderPath())) {
        std::error_code ec;
        std::filesystem::create_directories(GetRandomizerFolderPath(), ec);
        if (ec) {
            // "Generate New Seed" is already in the list; leave it as the only
            // option rather than letting directory_iterator throw.
            fprintf(stderr, "[MM] spoiler: cannot create spoiler directory '%s' (%s)\n",
                    GetRandomizerFolderPath().string().c_str(), ec.message().c_str());
            CVarSetInteger("gRando.SpoilerFileIndex", 0);
            CVarSetString("gRando.SpoilerFile", "");
            return;
        }
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
