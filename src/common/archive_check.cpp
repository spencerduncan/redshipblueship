/**
 * @file archive_check.cpp
 * @brief ROM-archive availability checks for the single executable (issue #317)
 *
 * See archive_check.h for the rationale. The search order here mirrors
 * Ship::Context::LocateFileAcrossAppDirs so the message shows exactly the
 * paths the game itself would have probed.
 */

#include "archive_check.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL2/SDL_messagebox.h>
#include <ship/Context.h>

namespace {

struct GameArchiveSpec {
    const char* displayName;             // user-facing game name
    const char* appName;                 // app-dir key for LocateFileAcrossAppDirs
    std::vector<const char*> candidates; // accepted archive filenames, preferred first
    const char* howToFix;                // instructions appended to the message
};

const GameArchiveSpec* SpecFor(GameId game) {
    static const GameArchiveSpec sOotSpec = {
        "Ocarina of Time",
        "soh",
        { "oot.o2r", "oot-mq.o2r" },
        "To set it up, either:\n"
        "  - Restart RedShipBlueShip with \"--game oot\" and let the built-in\n"
        "    extractor generate oot.o2r from your Ocarina of Time ROM, or\n"
        "  - Copy an existing oot.o2r next to the redship executable.\n",
    };
    static const GameArchiveSpec sMmSpec = {
        "Majora's Mask",
        "2s2h",
        { "mm.o2r", "mm.zip", "mm.otr" },
        "mm.o2r is generated from your own Majora's Mask ROM\n"
        "(US N64 1.0 or US GameCube version). To set it up, either:\n"
        "  - Relaunch RedShipBlueShip, select Majora's Mask, and accept the\n"
        "    generation prompt — the built-in extractor creates mm.o2r from\n"
        "    your ROM (needs the packaged assets folder next to the\n"
        "    executable), or\n"
        "  - Generate mm.o2r elsewhere (standalone 2Ship2Harkinian, or a\n"
        "    build of this repository — see docs/mm-archive-setup.md) and\n"
        "    copy it next to the redship executable, then relaunch.\n",
    };

    switch (game) {
        case GAME_OOT:
            return &sOotSpec;
        case GAME_MM:
            return &sMmSpec;
        default:
            return nullptr;
    }
}

// The three locations LocateFileAcrossAppDirs probes, in its search order.
std::vector<std::string> SearchPathsFor(const char* filename, const char* appName) {
    return {
        Ship::Context::GetPathRelativeToAppDirectory(filename, appName),
        Ship::Context::GetPathRelativeToAppBundle(filename),
        "./" + std::string(filename),
    };
}

bool ArchiveExists(const char* filename, const char* appName) {
    for (const auto& path : SearchPathsFor(filename, appName)) {
        std::error_code ec;
        if (!path.empty() && std::filesystem::exists(path, ec)) {
            return true;
        }
    }
    return false;
}

// Port-asset archive locations. MUST stay in sync with LoadMMArchives
// (games/mm/2s2h/GameExports_SingleExe.cpp): a location this check accepts
// but the loader does not probe would pass the gate and still boot MM into
// null-resource territory. Deliberately NOT the per-app data directory — a
// standalone 2Ship2Harkinian install's 2ship.o2r there would be
// version-mismatched with this build's MM.
std::vector<std::string> PortArchiveSearchPaths(const char* filename) {
    return {
        Ship::Context::GetPathRelativeToAppBundle(filename),
        "./" + std::string(filename),
    };
}

} // namespace

extern "C" bool ArchiveCheck_GameAvailable(GameId game) {
    const GameArchiveSpec* spec = SpecFor(game);
    if (spec == nullptr) {
        return true;
    }
    for (const char* filename : spec->candidates) {
        if (ArchiveExists(filename, spec->appName)) {
            return true;
        }
    }
    return false;
}

extern "C" bool ArchiveCheck_PortArchiveAvailable(GameId game) {
    if (game != GAME_MM) {
        return true;
    }
    for (const auto& path : PortArchiveSearchPaths("2ship.o2r")) {
        std::error_code ec;
        if (!path.empty() && std::filesystem::exists(path, ec)) {
            return true;
        }
    }
    return false;
}

extern "C" void ArchiveCheck_ReportMissingPortArchive(GameId game, bool showDialog) {
    if (game != GAME_MM) {
        return;
    }

    std::string msg =
        "Majora's Mask cannot start: the port-asset archive \"2ship.o2r\" was not found.\n\n"
        "RedShipBlueShip looked in:\n";
    for (const auto& path : PortArchiveSearchPaths("2ship.o2r")) {
        msg += "  " + path + "\n";
    }
    msg += "\n"
           "Unlike mm.o2r, 2ship.o2r is not generated from your ROM — it ships\n"
           "with RedShipBlueShip itself (fonts and port assets). Your download or\n"
           "install is incomplete, or it came from a package that was missing the\n"
           "file. Re-download the latest RedShipBlueShip package and make sure\n"
           "2ship.o2r sits next to the redship executable.\n";

    const char* title = "Majora's Mask port archive missing";

    fprintf(stderr, "\n[RSBS] === %s ===\n%s\n", title, msg.c_str());
    fflush(stderr);

    if (showDialog) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg.c_str(), nullptr);
    }
}

extern "C" void ArchiveCheck_ReportMissing(GameId game, bool showDialog) {
    const GameArchiveSpec* spec = SpecFor(game);
    if (spec == nullptr) {
        return;
    }

    std::string msg = std::string(spec->displayName) + " is not playable yet: no \"" + spec->candidates[0] +
                      "\" game archive was found.\n\nRedShipBlueShip looked in:\n";
    for (const auto& path : SearchPathsFor(spec->candidates[0], spec->appName)) {
        msg += "  " + path + "\n";
    }
    if (spec->candidates.size() > 1) {
        msg += "(also accepted:";
        for (size_t i = 1; i < spec->candidates.size(); i++) {
            msg += std::string(" ") + spec->candidates[i];
        }
        msg += ")\n";
    }
    msg += "\n";
    msg += spec->howToFix;

    std::string title = std::string(spec->displayName) + " archive missing";

    fprintf(stderr, "\n[RSBS] === %s ===\n%s\n", title.c_str(), msg.c_str());
    fflush(stderr);

    if (showDialog) {
        // Works before SDL_Init/window creation; on headless systems it fails
        // silently and the stderr text above is the fallback.
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), msg.c_str(), nullptr);
    }
}
