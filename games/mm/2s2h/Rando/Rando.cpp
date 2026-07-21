#include "Rando.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "Rando/ActorBehavior/ActorBehavior.h"
#include "Rando/MiscBehavior/MiscBehavior.h"
#include "Rando/MiscBehavior/ClockShuffle.h"
#include "Rando/Spoiler/Spoiler.h"
#include "Rando/CheckTracker/CheckTracker.h"
#include "2s2h/ShipInit.hpp"
#include <ship/window/FileDropMgr.h>
#include <ship/Context.h>

// When a save is loaded, we want to unregister all hooks and re-register them if it's a rando save
void OnSaveLoadHandler(s16 fileNum) {
    Rando::MiscBehavior::OnFileLoad();
    Rando::ActorBehavior::OnFileLoad();
    Rando::CheckTracker::OnFileLoad();
    Rando::ClockShuffle::OnFileLoad();

    // Re-initalizes enhancements that are effected by the save being rando or not
    ShipInit::Init("IS_RANDO");
}

// Entry point for the module, run once on game boot
void Rando::Init() {
    Rando::Spoiler::RefreshOptions();
    Rando::MiscBehavior::Init();
    Rando::ActorBehavior::Init();
    // Single-exe: these calls are what pull CheckTracker.obj (and its S2H
    // UIWidgets) out of the plain 2ship_rando_ui archive; the window globals
    // it references live in 2s2h/TrackersGuiSingleExe.cpp, which registers
    // the tracker windows on the shared Gui without touching BenMenu.
    // Rando/Menu.cpp stays elided (#392).
    Rando::CheckTracker::Init();
    // Null-guarded for the headless unit harness (mm-rando-gen), which
    // brings up Ship::Context without a file-drop manager.
    auto fileDropMgr = Ship::Context::GetInstance()->GetFileDropMgr();
    if (fileDropMgr) {
        fileDropMgr->RegisterDropHandler(Rando::Spoiler::HandleFileDropped);
    }

    S2H::GameHooks::Register<GameInteractor::OnSaveLoad>(OnSaveLoadHandler);
}

RandoCheckId Rando::FindItemPlacement(RandoItemId randoItemId) {
    for (auto& [randoCheckId, check] : Rando::StaticData::Checks) {
        if (RANDO_SAVE_CHECKS[randoCheckId].randoItemId == randoItemId) {
            return randoCheckId;
        }
    }

    return RC_UNKNOWN;
}
