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
#ifndef RSBS_SINGLE_EXECUTABLE
    // Single-exe: check-tracker UI stays link-elided (see Rando::Init below).
    Rando::CheckTracker::OnFileLoad();
#endif
    Rando::ClockShuffle::OnFileLoad();

    // Re-initalizes enhancements that are effected by the save being rando or not
    ShipInit::Init("IS_RANDO");
}

// Entry point for the module, run once on game boot
void Rando::Init() {
    Rando::Spoiler::RefreshOptions();
    Rando::MiscBehavior::Init();
    Rando::ActorBehavior::Init();
#ifndef RSBS_SINGLE_EXECUTABLE
    // Single-exe: the MM check-tracker window is BenGui UI, which stays
    // link-elided with the rest of MM's menu (2ship_rando_ui — see
    // games/mm/CMakeLists.txt and the Lane B surface note on #392). Calling
    // into it here would drag the whole UIWidgets/BenMenu surface into the
    // link.
    Rando::CheckTracker::Init();
#endif
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
