#include "FileSelectEnhancements.h"

#include "soh/OTRGlobals.h"
#include "soh/SohGui/SohModals.h"
#include "soh/SohGui/SohGui.hpp"

#include <array>
#include <string>

std::array<std::string, LANGUAGE_MAX> RandomizerSettingsMenuText[RSM_MAX] = {
    {
        // English
        "Start Randomizer",
        // German
        "Randomizer starten",
        // French
        "Commencer le Randomizer",
    },
    {
        // English
        "Generate New Randomizer Seed",
        // German
        "Neuen Randomizer Seed generieren",
        // French
        "Générer une nouvelle seed pour le Randomizer",
    },
    {
        // English
        "Open Randomizer Settings",
        // German
        "Randomizer Optionen öffnen",
        // French
        "Ouvrir les paramètres du Randomizer",
    },
    {
        // English
        "Generating...",
        // German
        "Generiere...",
        // French
        "Génération en cours...",
    },
    { // English
      "No randomizer seed loaded.\nPlease generate one first"
#if defined(__WIIU__) || defined(__SWITCH__)
      ".",
#else
      ",\nor drop a spoiler log on the game window.",
#endif
      // German
      "Kein Randomizer Seed gefunden.\nBitte generiere zuerst einen"
#if defined(__WIIU__) || defined(__SWITCH__)
      ".",
#else
      ",\noder ziehe ein Spoiler Log\nauf das Spielfenster.",
#endif
      // French
      "Aucune Seed de Randomizer actuellement disponible.\nGénérez-en une dans les \"Randomizer Settings\""
#if (defined(__WIIU__) || defined(__SWITCH__))
      "."
#else
      "\nou glissez un spoilerlog sur la fenêtre du jeu."
#endif
    },
};

const char* SohFileSelect_GetSettingText(uint8_t optionIndex, uint8_t language) {
    return RandomizerSettingsMenuText[optionIndex][language].c_str();
}

#ifdef RSBS_SINGLE_EXECUTABLE
#include <cstdio>

// ForeignItemsSingleExe.cpp: the slot the last staged generation created, or
// negative when no creation has consumed it since the last generation began.
extern "C" int OoT_Creation_StagedGenerationConsumedBy(void);

// RSBS (#564 V2): a staged generation makes exactly one file. Once a file has
// used it, "Start Randomizer" is greyed as when no seed is loaded, and this line
// says why in place of "No randomizer seed loaded". %d is the file's number.
// A failed creation keeps SoH's line: its "Not created:" toast already asked
// for a new seed.
static const char* const kSeedUsedText[] = {
    // English
    "This seed was used for file %d.\nPlease generate a new one first.",
    // German
    "Seed für Datei %d verwendet.\nBitte neuen Seed generieren.",
    // French
    "Cette seed a servi pour le fichier %d.\nGénérez-en une nouvelle d'abord.",
};

const char* OoT_FileSelect_NoSeedText(uint8_t language) {
    const int slot = OoT_Creation_StagedGenerationConsumedBy();
    if (slot < 0) {
        return SohFileSelect_GetSettingText(RSM_NO_RANDOMIZER_GENERATED, language);
    }
    if (language > LANGUAGE_FRA) {
        language = LANGUAGE_ENG;
    }
    static char sLine[128];
    snprintf(sLine, sizeof(sLine), kSeedUsedText[language], slot + 1);
    return sLine;
}

// RSBS (#952): what tells two paired worlds apart, beside the icons that cover
// only OoT's seed and settings (src/common/combo_identity_display.h).
#include "combo_identity_display.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/util.h"

// z_parameter.c (functions.h, whose C++ half redefines `this`): SoH's
// file-select text, the font the randomizer menu's lines use.
extern "C" uint16_t Interface_DrawTextLine(GraphicsContext* gfx, char text[], int16_t x, int16_t y, uint16_t colorR,
                                           uint16_t colorG, uint16_t colorB, uint16_t colorA, float textScale,
                                           uint8_t textShadow);

namespace {

// The paired seed STRING the panes print: the one this context generated or
// loaded (SaveManager's "inputSeed"), only when it is the world's own seed.
int OoT_PairedSeedString(uint32_t sharedRandoSeed, char* out, size_t cap) {
    const auto ctx = Rando::Context::GetInstance();
    if (ctx == nullptr || out == nullptr || cap == 0) {
        return 0;
    }
    const std::string& seedString = ctx->GetSeedString();
    if (seedString.empty() || SohUtils::Hash(seedString) != sharedRandoSeed) {
        return 0;
    }
    snprintf(out, cap, "%s", seedString.c_str());
    return 1;
}

const bool sPairedSeedStringRegistered =
    (Combo_IdentityDisplay_SetSeedStringProvider(&OoT_PairedSeedString), true);

} // namespace

// Right of the five icons (64 + 40 * 4 + 24 = 248, y 10..34), two lines in the
// randomizer menu's own font, scale and shadow.
#define OOT_COMBO_IDENTITY_X 254
#define OOT_COMBO_IDENTITY_Y 11
#define OOT_COMBO_IDENTITY_SCALE 0.8f

void OoT_FileSelect_DrawComboIdentity(GraphicsContext* gfx, s32 slot, u8 alpha) {
    char text[RSBS_COMBO_IDENTITY_TEXT_CAP];
    int have = 0;
    if (slot < 0) {
        const auto ctx = Rando::Context::GetInstance();
        have = ctx != nullptr && ctx->IsSeedGenerated() &&
               Combo_IdentityDisplay_GeneratedText(ctx->GetSeed(), text, sizeof(text));
    } else {
        have = Combo_IdentityDisplay_SlotText(slot, text, sizeof(text));
    }
    if (!have || alpha == 0) {
        return;
    }
    Interface_DrawTextLine(gfx, text, OOT_COMBO_IDENTITY_X, OOT_COMBO_IDENTITY_Y, 255, 255, 255, alpha,
                           OOT_COMBO_IDENTITY_SCALE, true);
    Combo_IdentityDisplay_NoteDrawn(text);
}
#endif

void SohFileSelect_ShowPresetMenu() {
    SohGui::ShowEscMenu();
    CVarSetString(CVAR_SETTING("Menu.ActiveHeader"), "Settings");
    CVarSetString(CVAR_SETTING("Menu.SettingsSidebarSection"), "Presets");
    CVarSetInteger(CVAR_GENERAL("HasSeenPresetModal"), 1);
}

void SohFileSelect_DismissPresetModal() {
    CVarSetInteger(CVAR_GENERAL("HasSeenPresetModal"), 1);
}

void SohFileSelect_ShowPresetModal() {
    if (CVarGetInteger(CVAR_GENERAL("HasSeenPresetModal"), 0)) {
        return;
    }
    std::shared_ptr<SohModalWindow> modal = static_pointer_cast<SohModalWindow>(
        Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Modal Window"));
    if (modal->IsPopupOpen("Take a look at our presets!")) {
        modal->DismissPopup();
    } else {
        modal->RegisterPopup("Take a look at our presets!",
                             "\nHey there! Ship comes with a ton of options, but none of them are on by default,\n"
                             "even in randomizer. If you haven't already, we highly recommend applying the\n"
                             "\"Enhancements - Curated Randomizer\" preset for a great, curated out of the\n"
                             "box rando experience.\n"
                             "\n"
                             "Afterwards, consider taking a look at the rest of the ESC menu to further tweak\n"
                             "the experience to your liking!\n",
                             "Cool, show me the presets!", "Got it, just let me play!", SohFileSelect_ShowPresetMenu,
                             SohFileSelect_DismissPresetModal);
    }
}
