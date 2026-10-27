/**
 * ROM-free lock for Majora's Mask's sequence replacement (W6-AUDIO-SEQ, #1239). CTest row MMAudioEditorReplacement
 * (label redship), `--test mm-audio-editor-replacement`.
 *
 * What was broken: 2Ship's Enhancements/Audio/AudioEditor.cpp is excluded from the single exe, and the lookup MM's
 * audio core calls for every sequence it loads or plays (MM_AudioEditor_GetReplacementSeq: audio/lib/load.c,
 * seqplayer.c, sequence.c, code_8019AF00.c) was an identity stub in 2s2h/GameExports_SingleExe.cpp. No MM music,
 * fanfare or song could be replaced, and the two randomize-all triggers did nothing in Termina.
 *
 * Legs:
 *  1. A replacement stored under MM's own key family (RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX) is what the
 *     lookup returns; a sequence with no replacement returns itself.
 *  2. SoH's spelling of a sfxKey both games use (gAudioEditor.ReplacedSequences.NA_BGM_BOSS.value, which SoH's Battle
 *     Music tab writes for OoT's boss music) does not move MM's boss music.
 *  3. The original-sequence lookup still answers inside MM_sSeqFlags[] (NA_BGM_DISABLED resolves to 0).
 *  4. SoH's randomize-audio combobox reaches MM through its translator: "On Rando Gen Only" makes MM's
 *     randomizer-generation hook (S2H::GameHooks OnRandoSeedGeneration, dispatched by Rando/MiscBehavior/
 *     OnFileCreate.cpp) give every replaceable Background Music row a replacement, and "On New Scene" does not arm it.
 *
 * Every key it sets is cleared, the mode key is driven back to its prior value, and AudioCollection::Instance is put
 * back as it was found.
 */
#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_AudioEditorReplacement_RunHeadless(void) {
    printf("[TEST] mm-audio-editor-replacement: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <climits>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Enhancements/Audio/AudioCollection.h"
#include "2s2h/Enhancements/Audio/AudioEditor.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "cvar_shared_keys.h" // src/common
#include "global.h"
#include "sequence.h"

namespace {

constexpr const char* kModeKey = "gAudioEditor.RandomizeAudioGenModes"; // SoH's combobox (RandomizeOnMode)
constexpr int kModeOnNewScene = 1;                                      // RANDOMIZE_ON_NEW_SCENE
constexpr int kModeOnRandoGenOnly = 2;                                  // RANDOMIZE_ON_RANDO_GEN_ONLY

std::string MmValueKey(const std::string& sfxKey) {
    return std::string(RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX) + sfxKey + ".value";
}

/** Clears every MM replacement, lock and exclusion key, spelled from the defines (not through the collection, so the
 *  cleanup is right whichever family the collection composes). */
void ClearMmFamilies(const AudioCollection& collection) {
    for (const auto& [id, info] : collection.GetAllSequences()) {
        CVarClear(MmValueKey(info.sfxKey).c_str());
        CVarClear((std::string(RSBS_CVAR_MM_AUDIO_REPLACED_SEQUENCES_PREFIX) + info.sfxKey + ".locked").c_str());
        CVarClear((std::string(RSBS_CVAR_MM_AUDIO_EXCLUDED_PREFIX) + info.sfxKey).c_str());
    }
}

/** The replaceable Background Music rows a randomize-all must give a replacement (2Ship's RandomizeGroup filter). */
std::vector<std::string> ReplaceableWorldKeys(const AudioCollection& collection) {
    std::vector<std::string> keys;
    for (const auto& [id, info] : collection.GetAllSequences()) {
        if ((info.category & SEQ_BGM_WORLD) && info.canBeReplaced && id < MAX_AUTHENTIC_SEQID) {
            keys.push_back(info.sfxKey);
        }
    }
    return keys;
}

int CountSet(const std::vector<std::string>& sfxKeys) {
    int set = 0;
    for (const std::string& k : sfxKeys) {
        if (CVarGetInteger(MmValueKey(k).c_str(), INT_MIN) != INT_MIN) {
            set++;
        }
    }
    return set;
}

void DriveMode(int mode) {
    CVarSetInteger(kModeKey, mode);
    ShipInit::Init(kModeKey);
}

} // namespace

extern "C" int MM_AudioEditorReplacement_RunHeadless(void) {
    printf("[TEST] mm-audio-editor-replacement: MM's sequence lookup answers from MM's own replacement keys, never "
           "SoH's, and SoH's randomize combobox reaches MM (#1239)\n");

    const bool createdCollection = AudioCollection::Instance == nullptr;
    if (createdCollection) {
        AudioCollection::Instance = new AudioCollection();
    }
    AudioCollection& collection = *AudioCollection::Instance;
    const int savedMode = CVarGetInteger(kModeKey, INT_MIN);
    const std::string sohBossKey = "gAudioEditor.ReplacedSequences.NA_BGM_BOSS.value";
    const int savedSohBoss = CVarGetInteger(sohBossKey.c_str(), INT_MIN);
    ClearMmFamilies(collection);

    int failures = 0;

    // Leg 1: a set MM key is the answer; an unset one is the original.
    CVarSetInteger(MmValueKey("NA_BGM_TERMINA_FIELD").c_str(), NA_BGM_CLOCK_TOWER);
    const u16 replaced = MM_AudioEditor_GetReplacementSeq(NA_BGM_TERMINA_FIELD);
    if (replaced != NA_BGM_CLOCK_TOWER) {
        printf("[TEST] FAIL: leg 1: Termina Field replaced by Clock Tower under %s, but MM's lookup returned 0x%02X "
               "(want 0x%02X): MM's audio core never plays a replacement\n",
               MmValueKey("NA_BGM_TERMINA_FIELD").c_str(), replaced, NA_BGM_CLOCK_TOWER);
        failures++;
    }
    const u16 untouched = MM_AudioEditor_GetReplacementSeq(NA_BGM_CLOCK_TOWER);
    if (untouched != NA_BGM_CLOCK_TOWER) {
        printf("[TEST] FAIL: leg 1: Clock Tower has no replacement, but MM's lookup returned 0x%02X\n", untouched);
        failures++;
    }
    printf("[TEST] leg 1: Termina Field -> 0x%02X, Clock Tower -> 0x%02X\n", replaced, untouched);

    // Leg 2: SoH's own key for a shared sfxKey leaves MM alone.
    CVarSetInteger(sohBossKey.c_str(), NA_BGM_TERMINA_FIELD);
    const u16 boss = MM_AudioEditor_GetReplacementSeq(NA_BGM_BOSS);
    if (boss != NA_BGM_BOSS) {
        printf("[TEST] FAIL: leg 2: SoH's %s moved MM's boss music to 0x%02X: one game's tab sets the other's music\n",
               sohBossKey.c_str(), boss);
        failures++;
    }
    printf("[TEST] leg 2: with SoH's NA_BGM_BOSS replaced, MM's boss music stays 0x%02X\n", boss);

    // Leg 3: the original lookup indexes MM_sSeqFlags[], so it must stay inside it.
    const u16 originalOfDisabled = MM_AudioEditor_GetOriginalSeq(NA_BGM_DISABLED);
    const u16 originalOfField = MM_AudioEditor_GetOriginalSeq(NA_BGM_TERMINA_FIELD);
    if (originalOfDisabled != 0 || originalOfField != NA_BGM_TERMINA_FIELD) {
        printf("[TEST] FAIL: leg 3: original lookup gave 0x%X for NA_BGM_DISABLED (want 0) and 0x%X for Termina Field "
               "(want 0x%X)\n",
               originalOfDisabled, originalOfField, NA_BGM_TERMINA_FIELD);
        failures++;
    }

    // Leg 4: SoH's randomize combobox, through MM's randomizer-generation hook.
    ClearMmFamilies(collection);
    const std::vector<std::string> worldKeys = ReplaceableWorldKeys(collection);
    DriveMode(kModeOnNewScene);
    S2H::GameHooks::Execute<GameInteractor::OnRandoSeedGeneration>();
    const int setOnNewScene = CountSet(worldKeys);
    if (setOnNewScene != 0) {
        printf("[TEST] FAIL: leg 4: with SoH's mode On New Scene, MM's randomizer-generation hook randomized %d "
               "Background Music rows (want 0)\n",
               setOnNewScene);
        failures++;
    }
    DriveMode(kModeOnRandoGenOnly);
    S2H::GameHooks::Execute<GameInteractor::OnRandoSeedGeneration>();
    const int setOnRandoGen = CountSet(worldKeys);
    if (worldKeys.empty() || setOnRandoGen != (int)worldKeys.size()) {
        printf("[TEST] FAIL: leg 4: with SoH's mode On Rando Gen Only, MM's randomizer generation gave %d of %zu "
               "replaceable Background Music rows a replacement (want all): the combobox does not reach MM\n",
               setOnRandoGen, worldKeys.size());
        failures++;
    }
    printf("[TEST] leg 4: On New Scene -> %d rows randomized on generation, On Rando Gen Only -> %d of %zu\n",
           setOnNewScene, setOnRandoGen, worldKeys.size());

    // Put everything back.
    ClearMmFamilies(collection);
    if (savedSohBoss == INT_MIN) {
        CVarClear(sohBossKey.c_str());
    } else {
        CVarSetInteger(sohBossKey.c_str(), savedSohBoss);
    }
    DriveMode(savedMode == INT_MIN ? 0 : savedMode);
    if (savedMode == INT_MIN) {
        CVarClear(kModeKey);
    }
    if (createdCollection) {
        delete AudioCollection::Instance;
        AudioCollection::Instance = nullptr;
    }

    if (failures != 0) {
        printf("[TEST] mm-audio-editor-replacement: FAIL (%d)\n", failures);
        return 1;
    }
    printf("[TEST] mm-audio-editor-replacement: PASS\n");
    return 0;
}

#endif
