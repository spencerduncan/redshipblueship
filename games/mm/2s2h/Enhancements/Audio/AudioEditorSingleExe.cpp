/**
 * Majora's Mask's sequence editor in the single exe (W6-AUDIO-SEQ, #1239, #1237).
 *
 * 2Ship's Enhancements/Audio/AudioEditor.cpp stays excluded: its window is BenGui's, it registers its hooks through the
 * raw GameInteractor instance, and its global names (RandomizeGroup, ResetGroup, ReplayCurrentBGM, ...) are SoH's too.
 * This TU is the port of its write paths (AudioEditor.cpp:58-363, :655-744), behind the MmAudioEditor seam
 * (src/common/mm_audio_editor.h) that SoH's Audio Editor window draws its Majora's Mask tab from, plus the two
 * randomize-all triggers, translated from SoH's own combobox. 2Ship's popout (gWindows.AudioEditor) has no window
 * here: SoH's one Audio Editor is it.
 *
 * Keys: every replacement, lock and exclusion key is composed by S2H::AudioCollection under MM's own families
 * (RSBS_CVAR_MM_AUDIO_*_PREFIX), so nothing here spells one. MM's audio core reads them through
 * MM_AudioEditor_GetReplacementSeq (2s2h/GameExports_SingleExe.cpp).
 *
 * Decisions recorded in docs/enhancement-classification.md:
 *  - "All Groups" covers the seven groups the pane has tabs for. 2Ship's allTypes leaves out SEQ_BGM_SONGS (its Songs
 *    tab worked, its "All Groups" buttons skipped it) and lists SEQ_SFX and SEQ_VOICE, whose tabs 2Ship compiles out
 *    and whose only row (General SFX) cannot be replaced. Songs is in, the two empty groups are out.
 *  - 2Ship's Sound Effects and Voices tabs are compiled out in 2Ship itself (#if 0) and are not ported.
 *
 * Randomize triggers: SoH's "Automatically Randomize All Music and Sound Effects" combobox
 * (gAudioEditor.RandomizeAudioGenModes) is the one control. Its On New Scene arms MM's new-scene randomize
 * (VB_PLAY_TRANSITION_CS, dispatched by z_play.c through func_800EDDB0), its On Rando Gen Only arms MM's randomize on
 * randomizer generation (OnRandoSeedGeneration, dispatched by Rando/MiscBehavior/OnFileCreate.cpp when MM's randomizer
 * file is created). MM's own RandomizeAllOnNewScene / RandomizeAllOnRandoGen bools are kTranslatedKeys rows: nothing
 * writes them. Neither hook body reads the play state.
 *
 * Linked because SoH's Audio Editor (soh_enh, whole-archived) calls the seam, so its one RegisterShipInitFunc always
 * runs; mm_registrar_coverage_test.cpp's W6-AUDIO-SEQ phase attributes it by registry content.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "AudioEditor.h"
#include "AudioCollection.h"

#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <functions.h>
#include <sequence.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "context.h"         // src/common: Context_GetCurrentGame
#include "mm_audio_editor.h" // src/common: the seam SoH's Audio Editor draws from

extern "C" void Audio_ForceRestorePreviousBgm(void);
extern "C" void MM_PreviewSequence(u16 seqId);

static_assert(MmAudioEditor::kGroupWorld == SEQ_BGM_WORLD && MmAudioEditor::kGroupEvent == SEQ_BGM_EVENT &&
                  MmAudioEditor::kGroupBattle == SEQ_BGM_BATTLE && MmAudioEditor::kGroupOcarina == SEQ_OCARINA &&
                  MmAudioEditor::kGroupFanfare == SEQ_FANFARE && MmAudioEditor::kGroupInstrument == SEQ_INSTRUMENT &&
                  MmAudioEditor::kGroupSongs == SEQ_BGM_SONGS && MmAudioEditor::kGroupCustom == SEQ_BGM_CUSTOM &&
                  MmAudioEditor::kGroupCustomFanfare == SEQ_BGM_CUSTOM_FANFARE,
              "the seam's groups must be S2H::SeqType's bits");

namespace {

// The groups "All Groups" acts on: one per tab of the pane (see the header for SEQ_BGM_SONGS, SEQ_SFX and SEQ_VOICE).
constexpr std::array<SeqType, 7> kAllGroups = { SEQ_BGM_WORLD, SEQ_BGM_EVENT,  SEQ_BGM_BATTLE, SEQ_OCARINA,
                                                SEQ_FANFARE,   SEQ_INSTRUMENT, SEQ_BGM_SONGS };

AudioCollection& Collection() {
    // MM_Game_Init creates it on Majora's Mask's first start; the editor can be drawn before that.
    if (AudioCollection::Instance == nullptr) {
        AudioCollection::Instance = new AudioCollection();
    }
    return *AudioCollection::Instance;
}

bool MmRunning() {
    return Context_GetCurrentGame() == GAME_MM;
}

void SaveNextFrame() {
    auto context = Ship::Context::GetInstance();
    if (context == nullptr || context->GetWindow() == nullptr || context->GetWindow()->GetGui() == nullptr) {
        return;
    }
    context->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

uint16_t ActiveBgmId() {
    return MmRunning() ? AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN) : NA_BGM_DISABLED;
}

// Replays the current BGM, which looks its replacement up again (2Ship's ReplayCurrentBGM). Only while MM runs: its
// sequence players are not MM's to command otherwise.
void ReplayCurrentBgm() {
    const uint16_t current = ActiveBgmId();
    if (current == NA_BGM_DISABLED) {
        return;
    }
    SEQCMD_PLAY_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0, current);
}

// Replays the BGM when @p seqKey (an original id) is what is playing (2Ship's UpdateCurrentBGM).
void UpdateCurrentBgm(uint16_t seqKey, uint32_t group) {
    if (group != SEQ_BGM_WORLD) {
        return;
    }
    if (ActiveBgmId() == seqKey) {
        ReplayCurrentBgm();
    }
}

void RandomizeGroupKeys(SeqType type) {
    AudioCollection& collection = Collection();
    std::vector<u16> values;

    // An empty included set means the pool was never read.
    if (collection.GetIncludedSequences().empty()) {
        collection.InitializeShufflePool();
    }

    // Duplicates are added when there are not enough included sequences.
    while (values.size() < collection.CountSequencesByType(type)) {
        const size_t initialSize = values.size();
        for (const auto& seqData : collection.GetIncludedSequences()) {
            if ((seqData->category & type) && seqData->canBeUsedAsReplacement) {
                values.push_back(seqData->sequenceId);
            }
        }
        if (values.size() == initialSize) {
            return;
        }
    }
    if (values.empty()) {
        return;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(values.begin(), values.end(), g);
    for (const auto& [seqId, seqData] : collection.GetAllSequences()) {
        const std::string cvarKey = collection.GetCvarKey(seqData.sfxKey);
        const std::string cvarLockKey = collection.GetCvarLockKey(seqData.sfxKey);
        // Locked entries are not randomized, and only authentic sequences get a key.
        if ((seqData.category & type) && CVarGetInteger(cvarLockKey.c_str(), 0) == 0) {
            if ((((seqData.category & SEQ_BGM_CUSTOM) || seqData.category == SEQ_FANFARE) &&
                 seqData.sequenceId >= MAX_AUTHENTIC_SEQID) ||
                seqData.canBeReplaced == false) {
                continue;
            }
            if (!values.empty()) {
                CVarSetInteger(cvarKey.c_str(), values.back());
                values.pop_back();
            }
        }
    }
}

void ResetGroupKeys(SeqType type) {
    AudioCollection& collection = Collection();
    for (const auto& [defaultValue, seqData] : collection.GetAllSequences()) {
        if (seqData.category != type || (seqData.category == SEQ_FANFARE && defaultValue >= MAX_AUTHENTIC_SEQID)) {
            continue;
        }
        if (CVarGetInteger(collection.GetCvarLockKey(seqData.sfxKey).c_str(), 0) == 0) {
            CVarClear(collection.GetCvarKey(seqData.sfxKey).c_str());
        }
    }
}

void SetGroupLocks(SeqType type, int locked) {
    AudioCollection& collection = Collection();
    for (const auto& [defaultValue, seqData] : collection.GetAllSequences()) {
        if (seqData.category != type || (seqData.category == SEQ_FANFARE && defaultValue >= MAX_AUTHENTIC_SEQID)) {
            continue;
        }
        CVarSetInteger(collection.GetCvarLockKey(seqData.sfxKey).c_str(), locked);
    }
}

// One tab's bulk button (2Ship's Draw_SfxTab buttons): the change, a save, and a replay when it moved the
// Background Music that is playing.
template <typename F> void BulkTabChange(uint32_t group, F&& change) {
    AudioCollection& collection = Collection();
    const uint16_t current = ActiveBgmId();
    const uint16_t before = collection.GetReplacementSequence(current);
    change(static_cast<SeqType>(group));
    SaveNextFrame();
    if (group == SEQ_BGM_WORLD && before != collection.GetReplacementSequence(current)) {
        ReplayCurrentBgm();
    }
}

MmAudioEditor::Sequence Copy(const SequenceInfo& info) {
    return MmAudioEditor::Sequence{ info.sequenceId,    info.label,
                                    info.sfxKey,        static_cast<uint32_t>(info.category),
                                    info.canBeReplaced, info.canBeUsedAsReplacement };
}

uint16_t sPreviewPlaying = 0;

// SoH's "Automatically Randomize All Music and Sound Effects" combobox (RandomizeOnMode,
// soh/Enhancements/enhancementTypes.h), the one control for both games. The translators below are MM's
// RandomizeAllOnNewScene and RandomizeAllOnRandoGen (RSBS::kTranslatedKeys).
constexpr const char* kSohRandomizeModesKey = "gAudioEditor.RandomizeAudioGenModes";
constexpr int kSohRandomizeOnNewScene = 1;     // RANDOMIZE_ON_NEW_SCENE
constexpr int kSohRandomizeOnRandoGenOnly = 2; // RANDOMIZE_ON_RANDO_GEN_ONLY

bool RandomizeAllOnNewSceneFromSoh() {
    return CVarGetInteger(kSohRandomizeModesKey, 0) == kSohRandomizeOnNewScene;
}

bool RandomizeAllOnRandoGenFromSoh() {
    return CVarGetInteger(kSohRandomizeModesKey, 0) == kSohRandomizeOnRandoGenOnly;
}

} // namespace

// ---- 2Ship's AudioEditor.h API (MM_AudioEditor_* after include/mm_audio_prefix.h) ---------------------------------

void AudioEditor_RandomizeAll() {
    for (SeqType type : kAllGroups) {
        RandomizeGroupKeys(type);
    }
    SaveNextFrame();
    ReplayCurrentBgm();
}

void AudioEditor_RandomizeGroup(SeqType group) {
    RandomizeGroupKeys(group);
    SaveNextFrame();
    ReplayCurrentBgm();
}

void AudioEditor_ResetAll() {
    for (SeqType type : kAllGroups) {
        ResetGroupKeys(type);
    }
    SaveNextFrame();
    ReplayCurrentBgm();
}

void AudioEditor_ResetGroup(SeqType group) {
    ResetGroupKeys(group);
    SaveNextFrame();
    ReplayCurrentBgm();
}

void AudioEditor_LockAll() {
    for (SeqType type : kAllGroups) {
        SetGroupLocks(type, 1);
    }
    SaveNextFrame();
}

void AudioEditor_UnlockAll() {
    for (SeqType type : kAllGroups) {
        SetGroupLocks(type, 0);
    }
    SaveNextFrame();
}

// ---- The seam SoH's Audio Editor draws MM's tab from ----------------------------------------------------------------

namespace MmAudioEditor {

std::vector<Sequence> AllSequences() {
    std::vector<Sequence> out;
    for (const auto& [id, info] : Collection().GetAllSequences()) {
        out.push_back(Copy(info));
    }
    return out;
}

std::vector<Sequence> IncludedSequences() {
    AudioCollection& collection = Collection();
    collection.InitializeShufflePool();
    std::vector<Sequence> out;
    for (const SequenceInfo* info : collection.GetIncludedSequences()) {
        out.push_back(Copy(*info));
    }
    return out;
}

std::vector<Sequence> ExcludedSequences() {
    AudioCollection& collection = Collection();
    collection.InitializeShufflePool();
    std::vector<Sequence> out;
    for (const SequenceInfo* info : collection.GetExcludedSequences()) {
        out.push_back(Copy(*info));
    }
    return out;
}

void SetInShufflePool(uint16_t id, bool included) {
    AudioCollection& collection = Collection();
    collection.InitializeShufflePool();
    // The pool holds pointers into the collection's own map, so the entry is found there, not copied.
    const auto from = included ? collection.GetExcludedSequences() : collection.GetIncludedSequences();
    for (SequenceInfo* info : from) {
        if (info->sequenceId == id) {
            if (included) {
                collection.AddToShufflePool(info);
            } else {
                collection.RemoveFromShufflePool(info);
            }
            return;
        }
    }
}

bool TabListsSequence(const Sequence& sequence, uint32_t group) {
    if ((~sequence.category) & group) {
        return false;
    }
    // Custom sequences are not listed, nor a sequence that cannot be replaced.
    return !((((sequence.category & SEQ_BGM_CUSTOM) || sequence.category == SEQ_BGM_CUSTOM_FANFARE) &&
              sequence.id >= MAX_AUTHENTIC_SEQID) ||
             !sequence.canBeReplaced);
}

uint16_t CurrentReplacement(const Sequence& sequence) {
    AudioCollection& collection = Collection();
    const int value = CVarGetInteger(collection.GetCvarKey(sequence.sfxKey).c_str(), sequence.id);
    return collection.HasSequenceNum(static_cast<uint16_t>(value)) ? static_cast<uint16_t>(value) : sequence.id;
}

bool IsLocked(const Sequence& sequence) {
    return CVarGetInteger(Collection().GetCvarLockKey(sequence.sfxKey).c_str(), 0) == 1;
}

void SetReplacement(const Sequence& sequence, uint32_t group, uint16_t replacement) {
    CVarSetInteger(Collection().GetCvarKey(sequence.sfxKey).c_str(), replacement);
    SaveNextFrame();
    UpdateCurrentBgm(sequence.id, group);
}

void ResetOne(const Sequence& sequence) {
    AudioCollection& collection = Collection();
    CVarClear(collection.GetCvarKey(sequence.sfxKey).c_str());
    CVarClear(collection.GetCvarLockKey(sequence.sfxKey).c_str());
    SaveNextFrame();
    UpdateCurrentBgm(sequence.id, sequence.category);
}

void RandomizeOne(const Sequence& sequence, uint32_t group) {
    AudioCollection& collection = Collection();
    collection.InitializeShufflePool();
    std::vector<const SequenceInfo*> valid;
    for (const SequenceInfo* info : collection.GetIncludedSequences()) {
        if (info->category & group) {
            valid.push_back(info);
        }
    }
    if (valid.empty()) {
        return;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<size_t> pick(0, valid.size() - 1);
    CVarSetInteger(collection.GetCvarKey(sequence.sfxKey).c_str(), valid[pick(g)]->sequenceId);
    CVarClear(collection.GetCvarLockKey(sequence.sfxKey).c_str());
    SaveNextFrame();
    UpdateCurrentBgm(sequence.id, group);
}

void ToggleLock(const Sequence& sequence) {
    const std::string lockKey = Collection().GetCvarLockKey(sequence.sfxKey);
    if (CVarGetInteger(lockKey.c_str(), 0) == 1) {
        CVarClear(lockKey.c_str());
    } else {
        CVarSetInteger(lockKey.c_str(), 1);
    }
    SaveNextFrame();
}

void RandomizeGroup(uint32_t group) {
    BulkTabChange(group, [](SeqType type) { RandomizeGroupKeys(type); });
}

void ResetGroup(uint32_t group) {
    BulkTabChange(group, [](SeqType type) { ResetGroupKeys(type); });
}

void LockGroup(uint32_t group) {
    BulkTabChange(group, [](SeqType type) { SetGroupLocks(type, 1); });
}

void UnlockGroup(uint32_t group) {
    BulkTabChange(group, [](SeqType type) { SetGroupLocks(type, 0); });
}

void RandomizeAll() {
    AudioEditor_RandomizeAll();
}

void ResetAll() {
    AudioEditor_ResetAll();
}

void LockAll() {
    AudioEditor_LockAll();
}

void UnlockAll() {
    AudioEditor_UnlockAll();
}

bool IsRunning() {
    return MmRunning();
}

uint16_t ActiveBgm() {
    return ActiveBgmId();
}

uint16_t PreviewPlaying() {
    if (!MmRunning()) {
        sPreviewPlaying = 0;
    }
    return sPreviewPlaying;
}

void TogglePreview(uint16_t id, uint32_t group) {
    if (!MmRunning()) {
        sPreviewPlaying = 0;
        return;
    }
    if (sPreviewPlaying != 0) {
        Audio_ForceRestorePreviousBgm();
        sPreviewPlaying = 0;
        return;
    }
    if (group == SEQ_INSTRUMENT) {
        AudioOcarina_SetInstrument(id - INSTRUMENT_OFFSET);
        AudioOcarina_SetPlaybackSong(9, 1);
        return;
    }
    MM_PreviewSequence(id);
    sPreviewPlaying = id;
}

} // namespace MmAudioEditor

// ---- SoH's randomize combobox, translated onto MM's two randomize-all triggers
// ---------------------------------------

void RegisterMmAudioEditorRandomizeHooks() {
    COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, RandomizeAllOnNewSceneFromSoh(), { AudioEditor_RandomizeAll(); });
    COND_HOOK(OnRandoSeedGeneration, RandomizeAllOnRandoGenFromSoh(), []() { AudioEditor_RandomizeAll(); });
}

static RegisterShipInitFunc sInitMmAudioEditorRandomizeHooks(RegisterMmAudioEditorRandomizeHooks,
                                                             { kSohRandomizeModesKey });

#endif
