#pragma once

#include "libultraship/libultra/types.h"

#define GAME_REGION_NTSC 0
#define GAME_REGION_PAL 1

#define GAME_PLATFORM_N64 0
#define GAME_PLATFORM_GC 1

#ifdef __cplusplus
#include <memory>
#include <ship/resource/Resource.h>

std::shared_ptr<Ship::IResource> ResourceMgr_GetResourceByNameHandlingMQ(const char* path);

extern "C" {
#endif // __cplusplus
#include "z64animation.h"
#include "z64audio.h"
#include "z64bgcheck.h"
uint32_t ResourceMgr_IsGameMasterQuest();
uint32_t ResourceMgr_IsSceneMasterQuest(s16 sceneNum);
uint32_t ResourceMgr_GameHasMasterQuest();
uint32_t ResourceMgr_GameHasOriginal();
uint32_t ResourceMgr_GetNumGameVersions();
uint32_t ResourceMgr_GetGameVersion(int index);
uint32_t ResourceMgr_GetGamePlatform(int index);
uint32_t ResourceMgr_GetGameRegion(int index);
bool ResourceMgr_IsPalLoaded();
void ResourceMgr_LoadDirectory(const char* resName);
void ResourceMgr_UnloadResource(const char* resName);
char** ResourceMgr_ListFiles(const char* searchMask, int* resultSize);
// Archive-scoped variant for single-exe builds: returns only entries owned by
// the given game's archives ("oot" or "mm"). In standalone builds the filter
// is a no-op (only one game's archives are ever loaded).
char** ResourceMgr_ListFilesForGame(const char* gameTag, const char* searchMask, int* resultSize);
// #618 (#516 Phase 3): re-scan the shared ExtensionCache for ONE game's
// archives. OTRExtScanner runs once, inside OoT's InitOTR, when only OoT's
// archives are mounted — so every MM path is absent from the cache that
// ResourceMgr_FileExists / ResourceMgr_FileAltExists answer from, and MM's
// custom-asset checks (its HD gfxprint font, PlayerCustomFlipbooks' static
// FD/Deku/Goron faces) all read false. MM calls this once its own archives are
// mounted. Insert-only and idempotent — see the definition for why it cannot
// clobber the other game's entries. Returns the number of NEW keys added, or
// -1 when there is no live ResourceManager/ArchiveManager.
int Combo_ExtensionCache_ScanGame(const char* gameTag);
// Entry count of the shared ExtensionCache. For the #618 lock row, which proves
// a rescan is purely additive by exact size accounting: the size must grow by
// precisely the number of new keys the scan reported.
size_t Combo_ExtensionCache_Size(void);
uint8_t ResourceMgr_FileExists(const char* resName);
uint8_t ResourceMgr_FileAltExists(const char* resName);
void ResourceMgr_UnloadOriginalWhenAltExists(const char* resName);
uint8_t ResourceMgr_TexIsRaw(const char* texPath);
uint8_t ResourceMgr_ResourceIsBackground(char* texPath);
char* ResourceMgr_LoadJPEG(char* data, size_t dataSize);
uint16_t ResourceMgr_LoadTexWidthByName(char* texPath);
uint16_t ResourceMgr_LoadTexHeightByName(char* texPath);
char* ResourceMgr_LoadTexOrDListByName(const char* filePath);
char* ResourceMgr_LoadPlayerAnimByName(const char* animPath);
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path);
char* ResourceMgr_GetNameByCRC(uint64_t crc, char* alloc);
Gfx* ResourceMgr_LoadGfxByCRC(uint64_t crc);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
uint8_t ResourceMgr_FileIsCustomByName(const char* path);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_PatchCustomGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);
char* ResourceMgr_LoadArrayByNameAsVec3s(const char* path);
Vtx* ResourceMgr_LoadVtxByCRC(uint64_t crc);
Vtx* ResourceMgr_LoadVtxByName(char* path);
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
SequenceData ResourceMgr_LoadSeqByName(const char* path);
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);
SoundFontSample* ResourceMgr_LoadAudioSample(const char* path);
CollisionHeader* ResourceMgr_LoadColByName(const char* path);
bool ResourceMgr_IsAltAssetsEnabled();
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
void ResourceMgr_UnregisterSkeleton(SkelAnime* skelAnime);
void ResourceMgr_ClearSkeletons();
s32* ResourceMgr_LoadCSByName(const char* path);
int ResourceMgr_OTRSigCheck(char* imgData);
char* ResourceMgr_GetResourceDataByNameHandlingMQ(const char* path);
#ifdef __cplusplus
}
#endif // __cplusplus
