#include "ConfigUpdaters.h"
#include "ConfigMigrators.h"
#include "soh/Enhancements/audio/AudioCollection.h"

// src/common — the cross-game CVar classification manifest (ADR 0003 / the
// enhancement-classification inventory). Version 7's convergence rows and the
// 2Ship importer both read kConvergedKeys from here.
#include "cvar_shared_keys.h"

namespace SOH {
ConfigVersion1Updater::ConfigVersion1Updater() : ConfigVersionUpdater(1) {
}
ConfigVersion2Updater::ConfigVersion2Updater() : ConfigVersionUpdater(2) {
}
ConfigVersion3Updater::ConfigVersion3Updater() : ConfigVersionUpdater(3) {
}
ConfigVersion4Updater::ConfigVersion4Updater() : ConfigVersionUpdater(4) {
}
ConfigVersion5Updater::ConfigVersion5Updater() : ConfigVersionUpdater(5) {
}
ConfigVersion6Updater::ConfigVersion6Updater() : ConfigVersionUpdater(6) {
}
ConfigVersion7Updater::ConfigVersion7Updater() : ConfigVersionUpdater(7) {
}
ConfigVersion8Updater::ConfigVersion8Updater() : ConfigVersionUpdater(8) {
}

void ConfigVersion1Updater::Update(Ship::Config* conf) {
    if (conf->GetInt("Window.Width", 640) == 640) {
        conf->Erase("Window.Width");
    }
    if (conf->GetInt("Window.Height", 480) == 480) {
        conf->Erase("Window.Height");
    }
    if (conf->GetInt("Window.PositionX", 100) == 100) {
        conf->Erase("Window.PositionX");
    }
    if (conf->GetInt("Window.PositionY", 100) == 100) {
        conf->Erase("Window.PositionY");
    }
    if (conf->GetString("Window.GfxBackend", "") == "") {
        conf->Erase("Window.GfxBackend");
    }
    if (conf->GetString("Window.GfxApi", "") == "") {
        conf->Erase("Window.GfxApi");
    }
    if (conf->GetString("Window.AudioBackend", "") == "") {
        conf->Erase("Window.AudioBackend");
    }
    if (conf->GetBool("Window.Fullscreen.Enabled", false) == false) {
        conf->Erase("Window.Fullscreen.Enabled");
    }
    if (conf->GetInt("Window.Fullscreen.Width", 1920) == 1920) {
        conf->Erase("Window.Fullscreen.Width");
    }
    if (conf->GetInt("Window.Fullscreen.Height", 1080) == 1080) {
        conf->Erase("Window.Fullscreen.Height");
    }
    if (conf->GetInt("Shortcuts.Fullscreen", Ship::KbScancode::LUS_KB_F11) == Ship::KbScancode::LUS_KB_F10) {
        conf->Erase("Shortcuts.Fullscreen");
    }
    if (conf->GetInt("Shortcuts.Console", Ship::KbScancode::LUS_KB_OEM_3) == Ship::KbScancode::LUS_KB_OEM_3) {
        conf->Erase("Shortcuts.Console");
    }
    if (conf->GetString("Game.SaveName", "") == "") {
        conf->Erase("Game.SaveName");
    }
    if (conf->GetString("Game.Main Archive", "") == "") {
        conf->Erase("Game.Main Archive");
    }
    if (conf->GetString("Game.Patches Archive", "") == "") {
        conf->Erase("Game.Patches Archive");
    }
    if (CVarGetInteger("gDirtPathFix", 0) != 0) {
        CVarSetInteger(CVAR_Z_FIGHTING_MODE, CVarGetInteger("gDirtPathFix", 0));
        CVarClear("gDirtPathFix");
    }
    if (CVarGetInteger("gRandomizedEnemies", 0) != 0) {
        if (CVarGetInteger("gSeededRandomizedEnemies", 0)) {
            CVarSetInteger("gRandomizedEnemies", 2);
        }
    }
    CVarClear("gSeededRandomizedEnemies");
}

void ConfigVersion2Updater::Update(Ship::Config* conf) {
    CVarClearBlock("gAudioEditor.ReplacedSequences");
}

void ConfigVersion3Updater::Update(Ship::Config* conf) {
    conf->EraseBlock("Controllers");

    if (conf->GetNestedJson().contains("CVars") && conf->GetNestedJson()["CVars"].contains("gInjectItemCounts")) {
        CVarClear("gInjectItemCounts");
        CVarSetInteger("gEnhancements.InjectItemCounts.GoldSkulltula", 1);
        CVarSetInteger("gEnhancements.InjectItemCounts.HeartContainer", 1);
        CVarSetInteger("gEnhancements.InjectItemCounts.HeartPiece", 1);
    }

    // Migrate all audio settings to ints
    if (conf->GetNestedJson().contains("CVars") && conf->GetNestedJson()["CVars"].contains("gGameMasterVolume")) {
        CVarSetInteger("gSettings.Volume.Master", (int32_t)(CVarGetFloat("gGameMasterVolume", 1.0f) * 100));
        CVarClear("gGameMasterVolume");
    }
    if (conf->GetNestedJson().contains("CVars") && conf->GetNestedJson()["CVars"].contains("gMainMusicVolume")) {
        CVarSetInteger("gSettings.Volume.MainMusic", (int32_t)(CVarGetFloat("gMainMusicVolume", 1.0f) * 100));
        CVarClear("gMainMusicVolume");
    }
    if (conf->GetNestedJson().contains("CVars") && conf->GetNestedJson()["CVars"].contains("gSubMusicVolume")) {
        CVarSetInteger("gSettings.Volume.SubMusic", (int32_t)(CVarGetFloat("gSubMusicVolume", 1.0f) * 100));
        CVarClear("gSubMusicVolume");
    }
    if (conf->GetNestedJson().contains("CVars") && conf->GetNestedJson()["CVars"].contains("gSFXMusicVolume")) {
        CVarSetInteger("gSettings.Volume.SFX", (int32_t)(CVarGetFloat("gSFXMusicVolume", 1.0f) * 100));
        CVarClear("gSFXMusicVolume");
    }
    if (conf->GetNestedJson().contains("CVars") && conf->GetNestedJson()["CVars"].contains("gFanfareVolume")) {
        CVarSetInteger("gSettings.Volume.Fanfare", (int32_t)(CVarGetFloat("gFanfareVolume", 1.0f) * 100));
        CVarClear("gFanfareVolume");
    }

    for (Migration migration : version3Migrations) {
        if (migration.action == MigrationAction::Rename) {
            CVarCopy(migration.from.c_str(), migration.to.value().c_str());
        }
        CVarClear(migration.from.c_str());
    }
}

void ConfigVersion4Updater::Update(Ship::Config* conf) {
    for (Migration migration : version4Migrations) {
        if (migration.action == MigrationAction::Rename) {
            CVarCopy(migration.from.c_str(), migration.to.value().c_str());
        }
        CVarClear(migration.from.c_str());
    }
}

void ConfigVersion5Updater::Update(Ship::Config* conf) {
    // After removal of Vanilla, make sure it doesn't crash because of an out of range on the combobox
    if (CVarGetInteger("gRandoSettings.LogicRules", 0) == 2) {
        CVarSetInteger("gRandoSettings.LogicRules", 0);
    }
}

void ConfigVersion6Updater::Update(Ship::Config* conf) {
    for (Migration migration : version6Migrations) {
        if (migration.action == MigrationAction::Rename) {
            CVarCopy(migration.from.c_str(), migration.to.value().c_str());
        }
        CVarClear(migration.from.c_str());
    }
}

// ============================================================================
// Version 7 — cross-game key convergence (ADR 0003 §5.1, §6.2-6.3)
// ============================================================================

/**
 * Is this CVar set at all?
 *
 * NOT `CVarExists` — libultraship declares that in consolevariablebridge.h and
 * never defines it anywhere, so calling it is a link error rather than a
 * presence check. `CVarGet` is the real accessor and is what the bridge's own
 * Register* paths use to test presence.
 */
static bool CVarPresent(const char* name) {
    return CVarGet(name) != nullptr;
}

bool ShouldAdoptLegacyValue(bool legacyPresent, bool canonicalPresent) {
    // Nothing to adopt, or OoT's value already stands. Either way the legacy
    // key is cleared by the caller — the retired spelling goes away regardless
    // of who won, so this migration is not re-runnable and cannot oscillate.
    return legacyPresent && !canonicalPresent;
}

int32_t VolumePercentFromFloatScale(float scale) {
    if (!(scale > 0.0f)) { // also catches NaN, which compares false everywhere
        return 0;
    }
    if (scale > 1.0f) {
        return 100;
    }
    return (int32_t)(scale * 100.0f);
}

void ConfigVersion7Updater::Update(Ship::Config* conf) {
    // Character colours: pure renames, so the declarative table covers them.
    for (Migration migration : version7Migrations) {
        if (migration.action == MigrationAction::RenameIfAbsent) {
            if (ShouldAdoptLegacyValue(CVarPresent(migration.from.c_str()),
                                       CVarPresent(migration.to.value().c_str()))) {
                CVarCopy(migration.from.c_str(), migration.to.value().c_str());
            }
        } else if (migration.action == MigrationAction::Rename) {
            CVarCopy(migration.from.c_str(), migration.to.value().c_str());
        }
        CVarClear(migration.from.c_str());
    }

    // Audio volume: a rename AND a representation change. MM stored a float
    // scale; OoT stores integer percent, and every reader of the canonical key
    // calls CVarGetInteger. Copying the Float-typed CVar across would leave a
    // value that libultraship's type check rejects, so the slider would read
    // its default forever — the same silent-no-op this whole convergence
    // exists to remove. Same conflict rule as above: OoT's value wins.
    //
    // Driven by the shared manifest rather than a local list, so the importer
    // and this updater cannot disagree about what maps to what.
    for (const RSBS::ConvergedKey& key : RSBS::kConvergedKeys) {
        if (!key.scaledPercent) {
            continue; // handled by version7Migrations above
        }
        if (ShouldAdoptLegacyValue(CVarPresent(key.legacy), CVarPresent(key.canonical))) {
            CVarSetInteger(key.canonical, VolumePercentFromFloatScale(CVarGetFloat(key.legacy, 1.0f)));
        }
        CVarClear(key.legacy);
    }
}

// ============================================================================
// Version 8 — the inventory §5.3 convergence remainder (#462)
// ============================================================================

/**
 * 23 pure MM -> OoT renames (ADR 0004 inventory §5.3 groups A-D). Every row was
 * verified to share type, units, range and default across both games before
 * landing, so — unlike version 7's audio family — none needs a value transform;
 * RenameIfAbsent copies the stored value across verbatim. Same conflict rule as
 * version 7: OoT's value wins, the legacy spelling is cleared either way, so the
 * migration is a no-op on re-run and cannot oscillate.
 */
void ConfigVersion8Updater::Update(Ship::Config* conf) {
    for (Migration migration : version8Migrations) {
        if (migration.action == MigrationAction::RenameIfAbsent) {
            if (ShouldAdoptLegacyValue(CVarPresent(migration.from.c_str()),
                                       CVarPresent(migration.to.value().c_str()))) {
                CVarCopy(migration.from.c_str(), migration.to.value().c_str());
            }
        } else if (migration.action == MigrationAction::Rename) {
            CVarCopy(migration.from.c_str(), migration.to.value().c_str());
        }
        CVarClear(migration.from.c_str());
    }
}
} // namespace SOH
