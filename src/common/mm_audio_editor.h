#pragma once
/**
 * W6-AUDIO-SEQ (#1239): the seam between SoH's Audio Editor window, which draws Majora's Mask's music pane, and
 * Majora's Mask's sequence collection, which owns MM's sequences and their replacement keys.
 *
 * SoH's window cannot include MM's audio headers (both games declare an AudioCollection, a SequenceInfo and a SeqType,
 * kept apart only by MM's S2H namespace), and games/oot may not spell an MM key. So the pane reads plain copies of MM's
 * rows and calls these functions for every write; each is defined in
 * games/mm/2s2h/Enhancements/Audio/AudioEditorSingleExe.cpp, the port of 2Ship's editor write paths. C++ only.
 */
#ifdef __cplusplus

#include <cstdint>
#include <string>
#include <vector>

namespace MmAudioEditor {

/** MM's sequence groups: S2H::SeqType's bit values (the defining TU static_asserts each one). */
enum Group : uint32_t {
    kGroupWorld = 1u << 0,
    kGroupEvent = 1u << 1,
    kGroupBattle = 1u << 2,
    kGroupOcarina = 1u << 3,
    kGroupFanfare = 1u << 4,
    kGroupInstrument = 1u << 7,
    kGroupSongs = 1u << 9,
    kGroupCustom = kGroupWorld | kGroupEvent | kGroupBattle,
    kGroupCustomFanfare = kGroupFanfare | kGroupOcarina | kGroupSongs,
};

/** One row of MM's sequence collection, copied. */
struct Sequence {
    uint16_t id;
    std::string label;
    std::string sfxKey;
    uint32_t category;
    bool canBeReplaced;
    bool canBeUsedAsReplacement;
};

/** Every sequence MM's collection holds, in id order. Creates the collection on first use: the editor can be open
 *  before Majora's Mask has run. */
std::vector<Sequence> AllSequences();
/** The shuffle pool's two halves, each in label order. */
std::vector<Sequence> IncludedSequences();
std::vector<Sequence> ExcludedSequences();
/** Moves one sequence into (true) or out of (false) the shuffle pool. */
void SetInShufflePool(uint16_t id, bool included);

/** Does @p group's tab list @p sequence? (2Ship's Draw_SfxTab filter: replaceable, not a custom sequence.) */
bool TabListsSequence(const Sequence& sequence, uint32_t group);
/** The sequence @p sequence currently plays as (itself when it has no replacement). */
uint16_t CurrentReplacement(const Sequence& sequence);
bool IsLocked(const Sequence& sequence);

void SetReplacement(const Sequence& sequence, uint32_t group, uint16_t replacement);
void ResetOne(const Sequence& sequence);
void RandomizeOne(const Sequence& sequence, uint32_t group);
void ToggleLock(const Sequence& sequence);

void RandomizeGroup(uint32_t group);
void ResetGroup(uint32_t group);
void LockGroup(uint32_t group);
void UnlockGroup(uint32_t group);
void RandomizeAll();
void ResetAll();
void LockAll();
void UnlockAll();

/** True while Majora's Mask is the game running: its music can be previewed and a change replays the current BGM. */
bool IsRunning();
/** The sequence MM's main BGM player is playing, or 0xFFFF when Majora's Mask is not running. */
uint16_t ActiveBgm();
/** The sequence a preview button started, or 0. */
uint16_t PreviewPlaying();
/** Starts (or, when one is playing, stops) a preview of @p id. Does nothing unless Majora's Mask is running. */
void TogglePreview(uint16_t id, uint32_t group);

} // namespace MmAudioEditor

/** UI snapshot harness only: on the Audio Editor's next frame, bring its Majora's Mask tab to the front (true) or its
 *  first tab, Audio Options (false). Defined beside the pane in games/oot/soh/Enhancements/audio/AudioEditor.cpp. */
void SohAudioEditor_ShowMmPaneForTest(bool show);

#endif
