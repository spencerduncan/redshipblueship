/**
 * MM scene-command parse regression test (issue #344).
 *
 * ROM-free coverage for the single-exe MM scene loader: feeds a synthetic
 * binary scene (the payload the OTR "Room" resource carries after its 64-byte
 * header) straight into MM's S2H::ResourceFactoryBinarySceneV0 and asserts the
 * MM-specific wire format parses:
 *
 *   - SetRoomBehavior (0x08) reads MM's 6-byte payload. OoT's parser reads
 *     5 bytes for the same opcode, which desyncs the stream — the core reason
 *     OoT's factory cannot parse MM scenes.
 *   - SetActorCutsceneList (0x1B) is an MM-only opcode with no OoT parser at
 *     all (OoT's factory returns a null command for it).
 *   - Opcode 0x19 is world-map-visited in MM (no payload); OoT would read a
 *     5-byte camera-settings payload.
 *   - EndMarker (0x14) terminates the command list.
 *
 * Parsing all four in sequence proves MM's command factories are registered
 * and stream-accurate. Included at FILE SCOPE by test_runner.cpp (compiled as
 * C++), like the other files in this directory.
 */

#include "2s2h/resource/importer/SceneFactory.h"
#include "2s2h/resource/type/2shResourceType.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/scenecommand/SceneCommand.h"
#include "2s2h/resource/type/scenecommand/SetRoomBehavior.h"
#include <ship/resource/File.h>
#include <ship/utils/binarytools/BinaryReader.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

static void MMSceneParse_PushU32(std::vector<char>& buf, uint32_t value) {
    // Little-endian, matching the reader endianness the test selects below.
    buf.push_back((char)(value & 0xFF));
    buf.push_back((char)((value >> 8) & 0xFF));
    buf.push_back((char)((value >> 16) & 0xFF));
    buf.push_back((char)((value >> 24) & 0xFF));
}

TestResult Test_MMSceneParse(void) {
    printf("[TEST] mm-scene-parse: MM scene commands parse via S2H factory (#344)\n");

    auto buffer = std::make_shared<std::vector<char>>();

    // Command count, then four commands: each is an int32 opcode + payload.
    MMSceneParse_PushU32(*buffer, 4);

    // SetRoomBehavior (0x08): MM layout is six int8 fields.
    MMSceneParse_PushU32(*buffer, 0x08);
    const char roomBehaviorBytes[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    buffer->insert(buffer->end(), roomBehaviorBytes, roomBehaviorBytes + 6);

    // SetActorCutsceneList (0x1B, MM-only): u32 entry count + one 16-byte entry.
    MMSceneParse_PushU32(*buffer, 0x1B);
    MMSceneParse_PushU32(*buffer, 1);
    for (int i = 0; i < 16; i++) {
        buffer->push_back((char)i);
    }

    // SetWorldMapVisited (0x19): no payload in MM's wire format.
    MMSceneParse_PushU32(*buffer, 0x19);

    // EndMarker (0x14).
    MMSceneParse_PushU32(*buffer, 0x14);

    auto file = std::make_shared<Ship::File>();
    file->Buffer = buffer;
    auto reader = std::make_shared<Ship::BinaryReader>(buffer->data(), buffer->size());
    reader->SetEndianness(Ship::Endianness::Little);
    file->Reader = reader;
    file->IsLoaded = true;

    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Path = "test/mm-scene-parse";
    initData->Type = static_cast<uint32_t>(S2H::ResourceType::SOH_Room);
    initData->ResourceVersion = 0;
    initData->Format = RESOURCE_FORMAT_BINARY;
    initData->ByteOrder = Ship::Endianness::Little;

    S2H::ResourceFactoryBinarySceneV0 factory;
    auto resource = factory.ReadResource(file, initData);
    if (resource == nullptr) {
        printf("[TEST] FAIL: S2H scene factory returned null resource\n");
        return TEST_FAIL;
    }

    auto scene = std::static_pointer_cast<S2H::Scene>(resource);
    if (scene->commands.size() != 4) {
        printf("[TEST] FAIL: expected 4 parsed commands, got %zu\n", scene->commands.size());
        return TEST_FAIL;
    }

    for (size_t i = 0; i < scene->commands.size(); i++) {
        if (scene->commands[i] == nullptr) {
            printf("[TEST] FAIL: command %zu parsed as null (missing MM command factory)\n", i);
            return TEST_FAIL;
        }
    }

    const S2H::SceneCommandID expectedIds[4] = {
        S2H::SceneCommandID::SetRoomBehavior,
        S2H::SceneCommandID::SetActorCutsceneList,
        S2H::SceneCommandID::SetWorldMapVisited,
        S2H::SceneCommandID::EndMarker,
    };
    for (size_t i = 0; i < 4; i++) {
        if (scene->commands[i]->cmdId != expectedIds[i]) {
            printf("[TEST] FAIL: command %zu has cmdId 0x%X, expected 0x%X — MM parsers desynced the stream\n", i,
                   (unsigned)scene->commands[i]->cmdId, (unsigned)expectedIds[i]);
            return TEST_FAIL;
        }
    }

    // The 6-byte MM RoomBehavior payload must land field-for-field.
    auto roomBehavior = std::static_pointer_cast<S2H::SetRoomBehaviorMM>(scene->commands[0]);
    if (roomBehavior->roomBehavior.gameplayFlags != 0x11 || roomBehavior->roomBehavior.currRoomUnk2 != 0x22 ||
        roomBehavior->roomBehavior.currRoomUnk5 != 0x33 || roomBehavior->roomBehavior.msgCtxUnk != 0x44 ||
        roomBehavior->roomBehavior.enablePointLights != 0x55 || roomBehavior->roomBehavior.kankyoContextUnkE2 != 0x66) {
        printf("[TEST] FAIL: SetRoomBehavior fields wrong: %02X %02X %02X %02X %02X %02X\n",
               (unsigned char)roomBehavior->roomBehavior.gameplayFlags,
               (unsigned char)roomBehavior->roomBehavior.currRoomUnk2,
               (unsigned char)roomBehavior->roomBehavior.currRoomUnk5,
               (unsigned char)roomBehavior->roomBehavior.msgCtxUnk,
               (unsigned char)roomBehavior->roomBehavior.enablePointLights,
               (unsigned char)roomBehavior->roomBehavior.kankyoContextUnkE2);
        return TEST_FAIL;
    }

    printf("[TEST] PASS: 4 MM scene commands parsed with MM wire layouts\n");
    return TEST_PASS;
}
