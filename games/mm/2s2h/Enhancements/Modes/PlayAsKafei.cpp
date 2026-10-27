#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_test3/object_test3.h"

void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);
extern TexturePtr sPlayerEyesTextures[PLAYER_FORM_MAX][PLAYER_EYES_MAX];
extern TexturePtr sPlayerMouthTextures[PLAYER_FORM_MAX][PLAYER_MOUTH_MAX];
}

static SkeletonHeader gLinkHumanSkelBackup;
static SkeletonHeader gKafeiSkelBackup;

#define CVAR_NAME "gModes.PlayAsKafei"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

#ifdef RSBS_SINGLE_EXECUTABLE
// W6-MODES-DIFF (#1236): see the registrar below for why the single exe swaps lazily.
extern "C" bool MM_Rando_AssetsReady(void);
#include "context.h" // src/common: Context_GetCurrentGame

// Is Kafei's skeleton in the cached gLinkHumanSkel now? Keeps the scene-destroy hook armed after the switch turns off,
// so the next scene change puts Link back.
static bool sKafeiApplied = false;
// The two cached resources the swap edits and reads, held so the objects (and the limb tables the swapped header
// points into) outlive any unload of the cache, and so a reload is noticed: the backup is taken again from the fresh,
// unswapped copy. Heap-held and never freed: no static destructor runs at process exit.
struct KafeiSwapResources {
    std::shared_ptr<Ship::IResource> linkSkel;
    std::shared_ptr<Ship::IResource> kafeiSkel;
};
static KafeiSwapResources* sKafeiSwap = nullptr;

// Load both skeletons and take the backups the first time, or again after the cached copy changed. False when they
// cannot be loaded (no mm.o2r: the ROM-free rows, or the creation seam running MM's registrars before MM ever booted).
static bool PlayAsKafei_LoadBackups() {
    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    auto linkSkel = resourceManager->LoadResource(gLinkHumanSkel);
    auto kafeiSkel = resourceManager->LoadResource(gKafeiSkel);
    if (linkSkel == nullptr || kafeiSkel == nullptr || linkSkel->GetRawPointer() == nullptr ||
        kafeiSkel->GetRawPointer() == nullptr) {
        return false;
    }
    if (sKafeiSwap == nullptr) {
        sKafeiSwap = new KafeiSwapResources();
    }
    if (sKafeiSwap->linkSkel != linkSkel) {
        memcpy(&gLinkHumanSkelBackup, linkSkel->GetRawPointer(), sizeof(SkeletonHeader));
        sKafeiSwap->linkSkel = linkSkel;
        sKafeiApplied = false;
    }
    if (sKafeiSwap->kafeiSkel != kafeiSkel) {
        memcpy(&gKafeiSkelBackup, kafeiSkel->GetRawPointer(), sizeof(SkeletonHeader));
        sKafeiSwap->kafeiSkel = kafeiSkel;
    }
    return true;
}
#endif

void UpdatePlayAsKafei() {
#ifdef RSBS_SINGLE_EXECUTABLE
    if (!MM_Rando_AssetsReady() || !PlayAsKafei_LoadBackups()) {
        return;
    }
    if (!CVAR && !sKafeiApplied) {
        return; // nothing swapped, nothing to put back
    }
    sKafeiApplied = CVAR != 0;
#endif
    if (CVAR) {
        auto gLinkHumanSkelResource = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(gLinkHumanSkel);
        SkeletonHeader* gLinkHumanSkelPtr = (SkeletonHeader*)gLinkHumanSkelResource->GetRawPointer();
        memcpy(gLinkHumanSkelPtr, &gKafeiSkelBackup, sizeof(SkeletonHeader));

        ResourceMgr_PatchGfxByName(gLinkHumanWaistDL, "gLinkHumanWaistDL0", 0,
                                   gsSPDisplayListOTRFilePath(gKafeiWaistDL));
        ResourceMgr_PatchGfxByName(gLinkHumanWaistDL, "gLinkHumanWaistDL1", 1, gsSPEndDisplayList());

        sPlayerEyesTextures[PLAYER_FORM_HUMAN][0] = (TexturePtr)gKafeiEyesOpenTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][1] = (TexturePtr)gKafeiEyesHalfTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][2] = (TexturePtr)gKafeiEyesClosedTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][3] = (TexturePtr)gKafeiEyesRightTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][4] = (TexturePtr)gKafeiEyesLeftTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][5] = (TexturePtr)gKafeiEyesUpTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][6] = (TexturePtr)gKafeiEyesDownTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][7] = (TexturePtr)gKafeiEyesWincingTex;

        sPlayerMouthTextures[PLAYER_FORM_HUMAN][0] = (TexturePtr)gKafeiMouthClosedTex;
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][1] = (TexturePtr)gKafeiMouthHalfTex;
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][2] = (TexturePtr)gKafeiMouthOpenTex;
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][3] = (TexturePtr)gKafeiMouthSmileTex;
    } else {
        auto gLinkHumanSkelResource = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(gLinkHumanSkel);
        SkeletonHeader* gLinkHumanSkelPtr = (SkeletonHeader*)gLinkHumanSkelResource->GetRawPointer();
        memcpy(gLinkHumanSkelPtr, &gLinkHumanSkelBackup, sizeof(SkeletonHeader));

        ResourceMgr_UnpatchGfxByName(gLinkHumanWaistDL, "gLinkHumanWaistDL0");
        ResourceMgr_UnpatchGfxByName(gLinkHumanWaistDL, "gLinkHumanWaistDL1");

        sPlayerEyesTextures[PLAYER_FORM_HUMAN][0] = (TexturePtr)gLinkHumanEyesOpenTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][1] = (TexturePtr)gLinkHumanEyesHalfTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][2] = (TexturePtr)gLinkHumanEyesClosedTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][3] = (TexturePtr)gLinkHumanEyesRightTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][4] = (TexturePtr)gLinkHumanEyesLeftTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][5] = (TexturePtr)gLinkHumanEyesUpTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][6] = (TexturePtr)gLinkHumanEyesDownTex;
        sPlayerEyesTextures[PLAYER_FORM_HUMAN][7] = (TexturePtr)gLinkHumanEyesWincingTex;

        sPlayerMouthTextures[PLAYER_FORM_HUMAN][0] = (TexturePtr)gLinkHumanMouthClosedTex;
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][1] = (TexturePtr)gLinkHumanMouthHalfTex;
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][2] = (TexturePtr)gLinkHumanMouthOpenTex;
        sPlayerMouthTextures[PLAYER_FORM_HUMAN][3] = (TexturePtr)gLinkHumanMouthSmileTex;
    }
}

#ifdef RSBS_SINGLE_EXECUTABLE
// W6-MODES-DIFF (#1236): 2Ship's registrar runs once, at boot, and loads both skeletons at once. In the single exe the
// registrars first run from MM_Rando_InitCore, which the paired-file creation seam calls while Ocarina of Time runs and
// mm.o2r may not be mounted (LoadResource returns null), and the ROM-free rows call with no archive at all. And the
// resource cache has one owner per game (#981): a swap made while Ocarina of Time runs would edit a copy MM never
// draws. So the swap happens only from MM's own scene-destroy hook (MM runs, its owner holds), or here when MM is
// running with no scene yet (MM's own boot), never mid-scene. Keyed on the switch, so a change reaches the next scene
// change; the hook stays armed while Kafei's skeleton is in, so turning the switch off puts Link back.
void RegisterPlayAsKafei() {
    if (CVAR && Context_GetCurrentGame() == GAME_MM && MM_gPlayState == nullptr) {
        UpdatePlayAsKafei();
    }
    COND_HOOK(OnPlayDestroy, CVAR || sKafeiApplied, UpdatePlayAsKafei);
}

static RegisterShipInitFunc initFunc(RegisterPlayAsKafei, { CVAR_NAME });
#else
void RegisterPlayAsKafei() {
    // Even though this isn't run when a cvar is changed, it can still run if ShipInit::InitAll(); is called again,
    // likely in the case of setting a preset or something. So we need to make sure this only runs once.
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    auto gLinkHumanSkelResource = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(gLinkHumanSkel);
    auto gKafeiSkelResource = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(gKafeiSkel);

    SkeletonHeader* gLinkHumanSkelPtr = (SkeletonHeader*)gLinkHumanSkelResource->GetRawPointer();
    SkeletonHeader* gKafeiSkelPtr = (SkeletonHeader*)gKafeiSkelResource->GetRawPointer();

    memcpy(&gLinkHumanSkelBackup, gLinkHumanSkelPtr, sizeof(SkeletonHeader));
    memcpy(&gKafeiSkelBackup, gKafeiSkelPtr, sizeof(SkeletonHeader));

    UpdatePlayAsKafei();

    S2H::GameHooks::Register<GameInteractor::OnPlayDestroy>([]() { UpdatePlayAsKafei(); });
}

// We only want this running at boot, we don't want this running when the cvar is changed, only on scene destroy
static RegisterShipInitFunc initFunc(RegisterPlayAsKafei, {});
#endif
