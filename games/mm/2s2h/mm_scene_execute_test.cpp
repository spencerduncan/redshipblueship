/**
 * MM scene-command EXECUTE regression test (issue #344).
 *
 * Sibling to src/common/tests/test_mm_scene_parse.c. Where that test proves
 * MM's wire format PARSES into command objects, this one proves the parsed
 * commands EXECUTE correctly against a PlayState — the layer where the
 * single-exe MM scene loader deterministically null-derefs when a handler is
 * an unfaithful port. It is ROM-free and display-free, so it runs in the
 * hosted-CI "redship" CTest tier (the archive-gated int-boot-mm test, which is
 * the only other thing that reaches this code, never runs on hosted runners).
 *
 * Why this lives in an MM translation unit (and not, like the parse test,
 * #include'd at file scope into src/common/test_runner.cpp): executing commands
 * needs the real PlayState / ActorEntry / EntranceEntry / RomFile game types
 * from MM's global.h. Pulling MM's umbrella headers into test_runner.cpp's TU
 * (which already carries the OoT/common headers) risks macro/type collisions.
 * Instead the body compiles here — exactly the header set z_scene_2SH.cpp
 * already builds with — and is exposed through the C entry point
 * MM_SceneExecute_RunHeadless(), mirroring MM_RegisterResourceFactoriesHeadless
 * (GameExports_SingleExe.cpp). The undefined reference from test_runner.cpp
 * force-links this .o out of 2ship_port, which in turn pulls in
 * z_scene_2SH.cpp's executor + handlers + the extracted spawn helper.
 *
 * Only the side-effect-free ("safe") handler subset is exercised:
 *   SetEntranceList (0x06), SetRoomList (0x04, count 0), SetRoomBehavior
 *   (0x08, MM 6-byte), SetActorList (0x01), EndMarker (0x14).
 * SetStartPositionList (0x00) is deliberately NOT triggered: its handler calls
 * Object_SpawnPersistent and writes gActorOverlayTable[0].profile
 * (z_scene_2SH.cpp), which needs the object system and is not headless-safe.
 * Its #344 pointer-fusion — the actual crash computation — is instead asserted
 * in isolation via the extracted pure helper MM_Play_ResolveLinkActorEntry.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "BenPort.h"
#include "global.h"

#include "2s2h/resource/importer/SceneFactory.h"
#include "2s2h/resource/type/2shResourceType.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/scenecommand/SceneCommand.h"
#include "2s2h/resource/type/scenecommand/SetEntranceList.h"
#include "2s2h/resource/type/scenecommand/SetActorList.h"
#include "2s2h/resource/type/scenecommand/SetRoomList.h"
#include "2s2h/resource/type/scenecommand/SetRoomBehavior.h"

#include <ship/resource/File.h>
#include <ship/utils/binarytools/BinaryReader.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

// Executor + extracted spawn helper live in z_scene_2SH.cpp (same 2ship_port
// archive). Declared with the game C types, exactly as z_scene_2SH.cpp /
// z_play_2SH.cpp declare MM_OTRScene_ExecuteCommands.
s32 MM_OTRScene_ExecuteCommands(PlayState* play, S2H::Scene* scene);
ActorEntry* MM_Play_ResolveLinkActorEntry(EntranceEntry* setupEntranceList, s32 curSpawn, ActorEntry* spawnEntries);

// MM_Actor_SpawnEntry lives in z_actor.c (C linkage; not header-declared) — the
// historically-crashing spawn site the #344 NULL guard protects. The guard
// short-circuits before any actorCtx/overlay/object access, so it is directly
// unit-testable here.
extern "C" Actor* MM_Actor_SpawnEntry(ActorContext* actorCtx, ActorEntry* actorEntry, PlayState* play);

// First-room processor from z_play_2SH.cpp — its NULL-room guard returns before
// any executor/actor/environment access, so a zeroed PlayState exercises it
// safely.
extern "C" s32 MM_OTRfunc_800973FC(PlayState* play, RoomContext* roomCtx);

namespace {

void MMSceneExec_PushU32(std::vector<char>& buf, uint32_t value) {
    // Little-endian, matching the reader endianness the test selects below and
    // the framing test_mm_scene_parse.c uses (command count, then int32 opcode
    // + payload per command).
    buf.push_back((char)(value & 0xFF));
    buf.push_back((char)((value >> 8) & 0xFF));
    buf.push_back((char)((value >> 16) & 0xFF));
    buf.push_back((char)((value >> 24) & 0xFF));
}

} // namespace

// Returns 0 on PASS, non-zero on FAIL. test_runner.cpp wraps this into a
// TestResult so MM's global.h stays out of its translation unit.
extern "C" int MM_SceneExecute_RunHeadless(void) {
    printf("[TEST] mm-scene-execute: MM scene commands execute against a PlayState (#344)\n");

    auto buffer = std::make_shared<std::vector<char>>();

    // commandCount = 5
    MMSceneExec_PushU32(*buffer, 5);

    // Cmd0 SetEntranceList (0x06): 1 entry, spawn=1 room=0. spawn=1 (not 0)
    // makes the linkActorEntry index arithmetic non-trivial (catches a base/
    // off-by-one regression that a spawn=0 fixture would mask).
    MMSceneExec_PushU32(*buffer, 0x06);
    MMSceneExec_PushU32(*buffer, 1);   // numEntrances
    buffer->push_back((char)0x01);     // entry[0].spawn = 1 (int8)
    buffer->push_back((char)0x00);     // entry[0].room  = 0 (int8)

    // Cmd1 SetRoomList (0x04): numRooms=0 so the per-room loop (which would
    // ResourceLoad a room file) never runs — keeps the test archive-free.
    MMSceneExec_PushU32(*buffer, 0x04);
    MMSceneExec_PushU32(*buffer, 0);   // numRooms

    // Cmd2 SetRoomBehavior (0x08): MM 6-byte int8 payload, distinct sentinels.
    MMSceneExec_PushU32(*buffer, 0x08);
    const char roomBehaviorBytes[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    buffer->insert(buffer->end(), roomBehaviorBytes, roomBehaviorBytes + 6);

    // Cmd3 SetActorList (0x01): 1 actor, one 16-byte entry (contents irrelevant).
    MMSceneExec_PushU32(*buffer, 0x01);
    MMSceneExec_PushU32(*buffer, 1);   // numActors
    for (int i = 0; i < 16; i++) {
        buffer->push_back((char)0x00);
    }

    // Cmd4 EndMarker (0x14): terminates the execute loop.
    MMSceneExec_PushU32(*buffer, 0x14);

    auto file = std::make_shared<Ship::File>();
    file->Buffer = buffer;
    auto reader = std::make_shared<Ship::BinaryReader>(buffer->data(), buffer->size());
    reader->SetEndianness(Ship::Endianness::Little);
    file->Reader = reader;
    file->IsLoaded = true;

    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Path = "test/mm-scene-execute";
    initData->Type = static_cast<uint32_t>(S2H::ResourceType::SOH_Room);
    initData->ResourceVersion = 0;
    initData->Format = RESOURCE_FORMAT_BINARY;
    initData->ByteOrder = Ship::Endianness::Little;

    S2H::ResourceFactoryBinarySceneV0 factory;
    auto resource = factory.ReadResource(file, initData);
    if (resource == nullptr) {
        printf("[TEST] FAIL: S2H scene factory returned null resource\n");
        return 1;
    }
    auto scene = std::static_pointer_cast<S2H::Scene>(resource);
    if (scene->commands.size() != 5) {
        printf("[TEST] FAIL: expected 5 parsed commands, got %zu\n", scene->commands.size());
        return 1;
    }
    for (size_t i = 0; i < scene->commands.size(); i++) {
        if (scene->commands[i] == nullptr) {
            printf("[TEST] FAIL: command %zu parsed as null (missing MM command factory)\n", i);
            return 1;
        }
    }

    auto entranceCmd = std::static_pointer_cast<S2H::SetEntranceList>(scene->commands[0]);
    auto roomListCmd = std::static_pointer_cast<S2H::SetRoomList>(scene->commands[1]);
    auto actorCmd = std::static_pointer_cast<S2H::SetActorList>(scene->commands[3]);

    // Zeroed PlayState on the heap (large trivial C struct — value-init zeroes
    // it). Every safe handler only WRITES its fields, so full-zero satisfies
    // all preconditions. Do NOT call MM_OTRPlay_InitScene: it would run
    // Object_InitContext and execute the live sceneSegment.
    auto play = std::make_unique<PlayState>();
    play->curSpawn = 0;

    // Poison every output field so a correct post-execute value proves the
    // handler actually ran (rules out a residual zero-init matching by luck).
    play->setupEntranceList = reinterpret_cast<EntranceEntry*>((uintptr_t)0xDEAD0001);
    play->setupActorList = reinterpret_cast<ActorEntry*>((uintptr_t)0xDEAD0002);
    play->roomList.romFiles = reinterpret_cast<RomFile*>((uintptr_t)0xDEAD0003);
    play->linkActorEntry = reinterpret_cast<ActorEntry*>((uintptr_t)0xDEAD0004);
    play->numSetupActors = (s16)0x7EEE;  // numSetupActors is s16 — keep the poison in range
    play->roomList.count = 0x7F;
    play->actorCtx.halfDaysBit = 0x7F;
    play->roomCtx.curRoom.type = 0x7F;
    play->roomCtx.curRoom.environmentType = 0x7F;
    play->roomCtx.curRoom.lensMode = 0x7F;
    play->roomCtx.curRoom.enablePosLights = 0x7F;
    play->msgCtx.unk12044 = 0x7FFF;
    play->envCtx.stormState = 0x7F;

    MM_OTRScene_ExecuteCommands(play.get(), scene.get());

    // (1) EntranceList handler (0x06): setupEntranceList <- entrances.data().
    // This is the ordering prerequisite whose absence made SpawnList deref null
    // in #344 — the spawn resolution reads setupEntranceList[curSpawn] first.
    if (play->setupEntranceList == nullptr ||
        (void*)play->setupEntranceList != entranceCmd->GetRawPointer()) {
        printf("[TEST] FAIL: setupEntranceList not set to entrances.data() (%p vs %p)\n",
               (void*)play->setupEntranceList, entranceCmd->GetRawPointer());
        return 1;
    }
    if (play->setupEntranceList[0].spawn != 1) {
        printf("[TEST] FAIL: parsed entrance spawn index wrong: %u (expected 1)\n",
               (unsigned)play->setupEntranceList[0].spawn);
        return 1;
    }

    // (2) #344 spawn-path pointer fusion, isolated from the object-spawn tail:
    // &spawnEntries[setupEntranceList[curSpawn].spawn] with spawn=1 -> &[1].
    // Uses the real parsed setupEntranceList, so it also proves the parsed
    // spawn index feeds the arithmetic.
    ActorEntry localSpawns[2];
    ActorEntry* expected = &localSpawns[1];
    ActorEntry* got = MM_Play_ResolveLinkActorEntry(play->setupEntranceList, play->curSpawn, localSpawns);
    if (got != expected) {
        printf("[TEST] FAIL: MM_Play_ResolveLinkActorEntry returned %p, expected %p (#344 arithmetic)\n",
               (void*)got, (void*)expected);
        return 1;
    }

    // (3) RoomList handler (0x04): count <- numRooms (0), romFiles <- rooms.data().
    // Weaker than the Entrance/Actor checks: numRooms is 0 (a non-zero count
    // would make the factory ReadString a room filename, whose wire encoding we
    // keep out of this fixture, and would head toward the room-load path). With
    // an empty rooms vector data() is typically null, so this mainly proves the
    // handler RAN (poison 0xDEAD0003 cleared) and used GetPointer(); it does not
    // discriminate a mis-sourced null. The pointer-bearing handlers below carry
    // the strong pointer-identity checks.
    if (play->roomList.count != 0) {
        printf("[TEST] FAIL: roomList.count = %d, expected 0\n", (int)play->roomList.count);
        return 1;
    }
    if ((void*)play->roomList.romFiles != (void*)roomListCmd->GetPointer()) {
        printf("[TEST] FAIL: roomList.romFiles not set to rooms.data() (%p vs %p)\n",
               (void*)play->roomList.romFiles, (void*)roomListCmd->GetPointer());
        return 1;
    }

    // (4) RoomBehavior handler (0x08): the six MM int8 fields land field-for-
    // field. Distinct sentinels catch a field reorder or a regression to the
    // commented-out bit-slice logic (z_scene_2SH.cpp) — this is the handler the
    // fidelity map flagged "suspect" (depends on the exporter emitting masked
    // single-bit values).
    if (play->roomCtx.curRoom.type != 0x11 || play->roomCtx.curRoom.environmentType != 0x22 ||
        play->roomCtx.curRoom.lensMode != 0x33 || play->msgCtx.unk12044 != 0x44 ||
        play->roomCtx.curRoom.enablePosLights != 0x55 || play->envCtx.stormState != 0x66) {
        printf("[TEST] FAIL: RoomBehavior fields wrong: type=%02X env=%02X lens=%02X msg=%04X posLights=%02X storm=%02X\n",
               (unsigned)(uint8_t)play->roomCtx.curRoom.type,
               (unsigned)(uint8_t)play->roomCtx.curRoom.environmentType,
               (unsigned)(uint8_t)play->roomCtx.curRoom.lensMode,
               (unsigned)(uint16_t)play->msgCtx.unk12044,
               (unsigned)(uint8_t)play->roomCtx.curRoom.enablePosLights,
               (unsigned)(uint8_t)play->envCtx.stormState);
        return 1;
    }

    // (5) ActorList handler (0x01): numSetupActors <- numActors, setupActorList
    // <- actorList.data(), halfDaysBit cleared. Catches a count/pointer swap.
    if (play->numSetupActors != (s32)actorCmd->numActors) {
        printf("[TEST] FAIL: numSetupActors = %d, expected %u\n",
               (int)play->numSetupActors, (unsigned)actorCmd->numActors);
        return 1;
    }
    if ((void*)play->setupActorList != actorCmd->GetRawPointer()) {
        printf("[TEST] FAIL: setupActorList not set to actorList.data() (%p vs %p)\n",
               (void*)play->setupActorList, actorCmd->GetRawPointer());
        return 1;
    }
    if (play->actorCtx.halfDaysBit != 0) {
        printf("[TEST] FAIL: actorCtx.halfDaysBit = %d, expected 0\n", (int)play->actorCtx.halfDaysBit);
        return 1;
    }

    // === MM_Actor_SpawnEntry NULL guard (#344 crash-site lock) ===
    // Locks the fix for the deterministic player-spawn null: when a scene
    // resource fails to load, Play_Init drives Actor_InitContext ->
    // MM_Actor_SpawnEntry with a NULL linkActorEntry. The guard returns NULL
    // instead of dereferencing actorEntry->rot.x. It short-circuits before any
    // actorCtx/overlay/object access, so zeroed heap structs exercise it safely.
    {
        auto guard_play = std::make_unique<PlayState>();
        auto guardActorCtx = std::make_unique<ActorContext>();
        Actor* spawned = MM_Actor_SpawnEntry(guardActorCtx.get(), nullptr, guard_play.get());
        if (spawned != nullptr) {
            printf("[TEST] FAIL: MM_Actor_SpawnEntry(NULL) returned %p, expected NULL (#344 guard)\n",
                   (void*)spawned);
            return 1;
        }
        printf("[TEST] PASS: MM_Actor_SpawnEntry NULL guard locked\n");
    }

    // === First-room NULL guard (cross-switch crash containment) ===
    // Locks MM_OTRfunc_800973FC's containment for a first-room load failure:
    // with status==1 and a NULL roomRequestAddr it must return 1 (so
    // MM_Play_Init's busyloop terminates) WITHOUT running the command executor
    // on the NULL room — previously this called
    // MM_OTRScene_ExecuteCommands(play, NULL) and null-derefed before the
    // post-busyloop player guard could run. The early return also skips
    // func_80123140, whose unguarded player->actor.id read is the other
    // in-busyloop crash on a player-less PlayState.
    {
        auto rg_play = std::make_unique<PlayState>();
        RoomContext* rg_roomCtx = &rg_play->roomCtx;
        rg_roomCtx->status = 1;
        rg_roomCtx->roomRequestAddr = nullptr;
        s32 processed = MM_OTRfunc_800973FC(rg_play.get(), rg_roomCtx);
        if (processed != 1) {
            printf("[TEST] FAIL: NULL-room guard returned %d, expected 1 (busyloop would spin)\n", (int)processed);
            return 1;
        }
        if (rg_roomCtx->status != 0) {
            printf("[TEST] FAIL: NULL-room guard left status=%d, expected 0\n", (int)rg_roomCtx->status);
            return 1;
        }
        printf("[TEST] PASS: first-room NULL guard contained\n");
    }

    // === ObjectList (0x0B) via WIRE-BYTE (regression lock) ===
    // Factory: SetObjectListFactory.cpp:11-15 (ReadCommandId Int32, numObjects UInt32,
    // then numObjects x ReadUInt16). Handler: z_scene_2SH.cpp:173-198. On a zeroed
    // PlayState numPersistentEntries==numEntries==0, so the first loop (183-191) is
    // skipped and Actor_KillAllWithMissingObject never runs -- CONFIRMED headless-safe.
    {
        auto ol_buffer = std::make_shared<std::vector<char>>();
        MMSceneExec_PushU32(*ol_buffer, 1);      // commandCount = 1
        MMSceneExec_PushU32(*ol_buffer, 0x0B);   // SCENE_CMD_ID_OBJECT_LIST (ReadCommandId: Int32)
        MMSceneExec_PushU32(*ol_buffer, 2);      // numObjects (ReadUInt32)
        ol_buffer->push_back((char)0x10);        // objects[0] = 0x0010 (ReadUInt16, LE)
        ol_buffer->push_back((char)0x00);
        ol_buffer->push_back((char)0x20);        // objects[1] = 0x0020 (ReadUInt16, LE)
        ol_buffer->push_back((char)0x00);

        auto ol_file = std::make_shared<Ship::File>();
        ol_file->Buffer = ol_buffer;
        auto ol_reader = std::make_shared<Ship::BinaryReader>(ol_buffer->data(), ol_buffer->size());
        ol_reader->SetEndianness(Ship::Endianness::Little);
        ol_file->Reader = ol_reader;
        ol_file->IsLoaded = true;

        auto ol_initData = std::make_shared<Ship::ResourceInitData>();
        ol_initData->Path = "test/mm-scene-execute/objectlist";
        ol_initData->Type = static_cast<uint32_t>(S2H::ResourceType::SOH_Room);
        ol_initData->ResourceVersion = 0;
        ol_initData->Format = RESOURCE_FORMAT_BINARY;
        ol_initData->ByteOrder = Ship::Endianness::Little;

        S2H::ResourceFactoryBinarySceneV0 ol_factory;
        auto ol_resource = ol_factory.ReadResource(ol_file, ol_initData);
        if (ol_resource == nullptr) {
            printf("[TEST] FAIL: ObjectList scene factory returned null\n");
            return 1;
        }
        auto ol_scene = std::static_pointer_cast<S2H::Scene>(ol_resource);
        if (ol_scene->commands.size() != 1 || ol_scene->commands[0] == nullptr) {
            printf("[TEST] FAIL: ObjectList command failed to parse\n");
            return 1;
        }

        auto ol_play = std::make_unique<PlayState>();
        // numEntries / numPersistentEntries MUST stay 0 (zero-init): they gate the
        // first loop that would call Actor_KillAllWithMissingObject. Only poison the outputs.
        ol_play->objectCtx.slots[0].id = (s16)0x7EEE; // poison
        ol_play->objectCtx.slots[1].id = (s16)0x7EEE; // poison

        MM_OTRScene_ExecuteCommands(ol_play.get(), ol_scene.get());

        if (ol_play->objectCtx.numEntries != 2) {
            printf("[TEST] FAIL: ObjectList numEntries = %d, expected 2\n", (int)ol_play->objectCtx.numEntries);
            return 1;
        }
        if (ol_play->objectCtx.slots[0].id != (s16)-0x0010 || ol_play->objectCtx.slots[1].id != (s16)-0x0020) {
            printf("[TEST] FAIL: ObjectList ids wrong: [0]=%d [1]=%d (expected -16,-32)\n",
                   (int)ol_play->objectCtx.slots[0].id, (int)ol_play->objectCtx.slots[1].id);
            return 1;
        }
        if (ol_play->objectCtx.numPersistentEntries != 0) {
            printf("[TEST] FAIL: ObjectList numPersistentEntries = %d, expected 0\n",
                   (int)ol_play->objectCtx.numPersistentEntries);
            return 1;
        }
        printf("[TEST] PASS: ObjectList (0x0B) locked\n");
    }

    // === SkyboxSettings (0x11) via WIRE-BYTE (regression lock) ===
    // Factory: SetSkyboxSettingsFactory.cpp:12-15 (ReadCommandId Int32, then 4x ReadInt8:
    // unk, skyboxId, weather, indoors). Handler: z_scene_2SH.cpp:341-350. Keep unk==0 (the
    // area-texture gate); Scene_LoadAreaTextures is commented out (349) so it is inert here.
    {
        auto sb_buffer = std::make_shared<std::vector<char>>();
        MMSceneExec_PushU32(*sb_buffer, 1);    // commandCount = 1
        MMSceneExec_PushU32(*sb_buffer, 0x11); // SCENE_CMD_ID_SKYBOX_SETTINGS (ReadCommandId: Int32)
        sb_buffer->push_back((char)0x00);      // settings.unk (ReadInt8) - keep 0, headless-safe
        sb_buffer->push_back((char)0x05);      // settings.skyboxId (ReadInt8)
        sb_buffer->push_back((char)0x07);      // settings.weather  (ReadInt8)
        sb_buffer->push_back((char)0x02);      // settings.indoors  (ReadInt8)

        auto sb_file = std::make_shared<Ship::File>();
        sb_file->Buffer = sb_buffer;
        auto sb_reader = std::make_shared<Ship::BinaryReader>(sb_buffer->data(), sb_buffer->size());
        sb_reader->SetEndianness(Ship::Endianness::Little);
        sb_file->Reader = sb_reader;
        sb_file->IsLoaded = true;

        auto sb_initData = std::make_shared<Ship::ResourceInitData>();
        sb_initData->Path = "test/mm-scene-execute/skybox";
        sb_initData->Type = static_cast<uint32_t>(S2H::ResourceType::SOH_Room);
        sb_initData->ResourceVersion = 0;
        sb_initData->Format = RESOURCE_FORMAT_BINARY;
        sb_initData->ByteOrder = Ship::Endianness::Little;

        S2H::ResourceFactoryBinarySceneV0 sb_factory;
        auto sb_resource = sb_factory.ReadResource(sb_file, sb_initData);
        if (sb_resource == nullptr) {
            printf("[TEST] FAIL: Skybox scene factory returned null\n");
            return 1;
        }
        auto sb_scene = std::static_pointer_cast<S2H::Scene>(sb_resource);
        if (sb_scene->commands.size() != 1 || sb_scene->commands[0] == nullptr) {
            printf("[TEST] FAIL: Skybox command failed to parse\n");
            return 1;
        }

        auto sb_play = std::make_unique<PlayState>();
        sb_play->skyboxId = 0x7F;                      // poison
        sb_play->envCtx.skyboxConfig = 0x7F;           // poison
        sb_play->envCtx.changeSkyboxNextConfig = 0x7F; // poison
        sb_play->envCtx.lightMode = 0x7F;              // poison

        MM_OTRScene_ExecuteCommands(sb_play.get(), sb_scene.get());

        if (sb_play->skyboxId != (0x05 & 3)) {
            printf("[TEST] FAIL: skyboxId = %d, expected %d\n", (int)sb_play->skyboxId, (0x05 & 3));
            return 1;
        }
        if (sb_play->envCtx.skyboxConfig != 0x07 || sb_play->envCtx.changeSkyboxNextConfig != 0x07) {
            printf("[TEST] FAIL: skyboxConfig=%d next=%d, expected 7,7\n",
                   (int)sb_play->envCtx.skyboxConfig, (int)sb_play->envCtx.changeSkyboxNextConfig);
            return 1;
        }
        if (sb_play->envCtx.lightMode != 0x02) {
            printf("[TEST] FAIL: lightMode = %d, expected 2\n", (int)sb_play->envCtx.lightMode);
            return 1;
        }
        printf("[TEST] PASS: SkyboxSettings (0x11) locked\n");
    }

    printf("[TEST] PASS: mm-scene-execute — scene commands + ObjectList/Skybox locks + spawn-entry guard\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
