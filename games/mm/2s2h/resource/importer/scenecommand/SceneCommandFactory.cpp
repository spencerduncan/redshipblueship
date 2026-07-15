#include "2s2h/resource/importer/scenecommand/SceneCommandFactory.h"
#include "2s2h/resource/type/scenecommand/SceneCommand.h"

namespace S2H {
void SceneCommandFactoryBinaryV0::ReadCommandId(std::shared_ptr<S2H::ISceneCommand> command,
                                                std::shared_ptr<Ship::BinaryReader> reader) {
    command->cmdId = (SceneCommandID)reader->ReadInt32();
}
} // namespace S2H
