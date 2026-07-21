#include "Spoiler.h"
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <spdlog/spdlog.h>
#include "BenPort.h"

namespace Rando {

namespace Spoiler {

std::string SpoilerDirectory() {
#ifdef RSBS_SINGLE_EXECUTABLE
    // Resolve against the shared app directory (see Spoiler.h). Passing no
    // appName uses the live Ship::Context's short name, which in the combo is
    // the one context OoT created — so MM's spoilers land beside OoT's rather
    // than in a second app directory that portable installs never create.
    const std::string dir = Ship::Context::GetPathRelativeToAppDirectory("randomizer-mm");
#else
    const std::string dir = Ship::Context::GetPathRelativeToAppDirectory("randomizer", appShortName);
#endif
    std::error_code ec;
    // create_directories (not create_directory): the parent app directory can
    // be missing on a fresh portable install, and the non-recursive form fails
    // silently in that case, leaving the write below to throw.
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void SaveToFile(const std::string& fileName, nlohmann::json spoiler) {
    const std::string filePath = SpoilerDirectory() + "/" + fileName;

    std::ofstream fileStream(filePath);
    if (!fileStream.is_open()) {
        // Name the path in both the log and the exception: "Failed to open
        // spoiler file" with no path was the entirety of the operator's
        // diagnostic surface when the MM spoiler went missing (#439).
        fprintf(stderr, "[MM] spoiler: WRITE FAILED at '%s'\n", filePath.c_str());
        fflush(stderr);
        throw std::runtime_error("Failed to open spoiler file for writing: " + filePath);
    }

    fileStream << spoiler.dump(4);
    fileStream.close();

    // Unconditional breadcrumb: the operator must be able to find the spoiler
    // without knowing how app directories resolve on their platform.
    std::error_code absEc;
    const std::filesystem::path absPath = std::filesystem::absolute(std::filesystem::path(filePath), absEc);
    const std::string reportedPath = absEc ? filePath : absPath.string();
    fprintf(stderr, "[MM] spoiler: wrote '%s'\n", reportedPath.c_str());
    fflush(stderr);
    SPDLOG_INFO("MM spoiler written to {}", reportedPath);
}

nlohmann::json LoadFromFile(const std::string& fileName) {
    const std::string spoilerFilePath = SpoilerDirectory() + "/" + fileName;
    std::ifstream fileStream(spoilerFilePath);
    if (!fileStream.is_open()) {
        throw std::runtime_error("Failed to open spoiler file: " + spoilerFilePath);
    }

    nlohmann::json spoiler;
    try {
        fileStream >> spoiler;
    } catch (nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse spoiler file: " + std::string(e.what()));
    }

    if (!spoiler.contains("type") || spoiler["type"] != "2S2H_RANDO_SPOILER") {
        throw std::runtime_error("Spoiler file is not a valid spoiler file");
    }

    return spoiler;
}

} // namespace Spoiler

} // namespace Rando
