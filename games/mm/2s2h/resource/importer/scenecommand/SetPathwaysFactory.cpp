#include "2s2h/resource/importer/scenecommand/SetPathwaysFactory.h"
#include "2s2h/resource/type/scenecommand/SetPathways.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

namespace S2H {
std::shared_ptr<Ship::IResource> SetPathwaysMMFactory::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                                    std::shared_ptr<Ship::BinaryReader> reader) {
    auto setPathways = std::make_shared<SetPathwaysMM>(initData);

    ReadCommandId(setPathways, reader);

    uint32_t declaredPaths = reader->ReadUInt32();
    setPathways->paths.reserve(declaredPaths);
    for (uint32_t i = 0; i < declaredPaths; i++) {
        std::string pathFileName = reader->ReadString();
        auto path = std::static_pointer_cast<PathMM>(
            Ship::Context::GetInstance()->GetResourceManager()->LoadResourceProcess(pathFileName.c_str()));
        // #560: this sub-load can come back null — the archive layer reads one shared
        // zip_t with no synchronization, so a concurrent read can leave the buffer
        // zeroed and ReadResourceInitData then rejects it. Skip the entry instead of
        // calling GetPointer() through the null. Mirrors the guard the sibling
        // SetCutscenesFactory in this directory already has, and OoT's
        // SetPathwaysFactory. Every read above stays consumed, so the binary reader
        // does not desync for the scene's remaining commands.
        if (path == nullptr) {
            SPDLOG_ERROR("SetPathwaysMM: failed to load pathway resource \"{}\" for {}; skipping it", pathFileName,
                         initData->Path);
            continue;
        }
        setPathways->paths.push_back(path->GetPointer());
    }

    // Report what actually loaded, not what the file claimed.
    setPathways->numPaths = static_cast<uint32_t>(setPathways->paths.size());

    return setPathways;
}
} // namespace S2H
