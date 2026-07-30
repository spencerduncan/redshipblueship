#include "2s2h/resource/importer/scenecommand/SetAnimatedMaterialListFactory.h"
#include "2s2h/resource/type/scenecommand/SetAnimatedMaterialList.h"
#include "2s2h/resource/type/TextureAnimation.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

namespace S2H {

std::shared_ptr<Ship::IResource>
SetAnimatedMaterialListFactory::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                             std::shared_ptr<Ship::BinaryReader> reader) {
    auto setAnimatedMat = std::make_shared<SetAnimatedMaterialList>(initData);

    ReadCommandId(setAnimatedMat, reader);

    std::string str = reader->ReadString();
    const auto data = std::static_pointer_cast<TextureAnimation>(
        Ship::Context::GetInstance()->GetResourceManager()->LoadResourceProcess(str.c_str()));

    // #560: a failed sub-load returns null. play->sceneMaterialAnims is null in a
    // scene that has no SetAnimatedMaterialList at all and its consumers handle that,
    // so store null rather than dereferencing.
    if (data == nullptr) {
        SPDLOG_ERROR("SetAnimatedMaterialList: failed to load texture animation \"{}\" for {}; leaving it unset", str,
                     initData->Path);
        setAnimatedMat->mat = nullptr;
        return setAnimatedMat;
    }

    AnimatedMaterial* res = data->GetPointer();
    setAnimatedMat->mat = res;

    return setAnimatedMat;
}
} // namespace S2H
