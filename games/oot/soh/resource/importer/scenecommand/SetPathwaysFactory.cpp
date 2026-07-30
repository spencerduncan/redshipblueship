#include "soh/resource/importer/scenecommand/SetPathwaysFactory.h"
#include "soh/resource/type/scenecommand/SetPathways.h"
#include "soh/resource/logging/SceneCommandLoggers.h"
#include "spdlog/spdlog.h"
#include <tinyxml2.h>
#include <libultraship/libultraship.h>

namespace SOH {
std::shared_ptr<Ship::IResource> SetPathwaysFactory::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                                  std::shared_ptr<Ship::BinaryReader> reader) {
    auto setPathways = std::make_shared<SetPathways>(initData);

    ReadCommandId(setPathways, reader);

    uint32_t declaredPaths = reader->ReadUInt32();
    setPathways->paths.reserve(declaredPaths);
    for (uint32_t i = 0; i < declaredPaths; i++) {
        std::string pathFileName = reader->ReadString();
        auto path = std::static_pointer_cast<Path>(
            Ship::Context::GetInstance()->GetResourceManager()->LoadResourceProcess(pathFileName.c_str()));
        // #560: this sub-load can legitimately come back null. The archive layer
        // reads one shared zip_t with no synchronization, so a concurrent read on
        // another thread can leave this buffer zeroed, and ReadResourceInitData
        // then rejects it (type/format/version 0). Calling GetPointer() through
        // that null shared_ptr was the operator's access violation. Skip the entry
        // instead so the scene loads with a short pathway list.
        if (path == nullptr) {
            SPDLOG_ERROR("SetPathways: failed to load pathway resource \"{}\" for {}; skipping it", pathFileName,
                         initData->Path);
            continue;
        }
        setPathways->paths.push_back(path->GetPointer());
        setPathways->pathFileNames.push_back(pathFileName);
    }

    // Report what actually loaded, not what the file claimed, so nothing walks off
    // the end of a list that lost entries above.
    setPathways->numPaths = static_cast<uint32_t>(setPathways->paths.size());

    if (CVarGetInteger(CVAR_DEVELOPER_TOOLS("ResourceLogging"), 0)) {
        LogPathwaysAsXML(setPathways);
    }

    return setPathways;
}

std::shared_ptr<Ship::IResource> SetPathwaysFactoryXML::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                                     tinyxml2::XMLElement* reader) {
    auto setPathways = std::make_shared<SetPathways>(initData);

    setPathways->cmdId = SceneCommandID::SetPathways;

    auto child = reader->FirstChildElement();

    while (child != nullptr) {
        std::string childName = child->Name();
        if (childName == "Pathway") {
            std::string pathFileName = child->Attribute("FilePath");
            auto path = std::static_pointer_cast<Path>(
                Ship::Context::GetInstance()->GetResourceManager()->LoadResourceProcess(pathFileName.c_str()));
            // Same guard as the binary reader above (#560).
            if (path == nullptr) {
                SPDLOG_ERROR("SetPathways: failed to load pathway resource \"{}\" for {}; skipping it", pathFileName,
                             initData->Path);
                child = child->NextSiblingElement();
                continue;
            }
            setPathways->paths.push_back(path->GetPointer());
            setPathways->pathFileNames.push_back(pathFileName);
        }

        child = child->NextSiblingElement();
    }

    setPathways->numPaths = static_cast<uint32_t>(setPathways->paths.size());

    return setPathways;
}
} // namespace SOH
