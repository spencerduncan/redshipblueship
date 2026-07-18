#pragma once
// Single-exe: renames the extern "C" AudioCollection_* wrappers below to
// MM_-prefixed symbols so MM's audio core links against MM's implementation
// instead of SoH's identically-named one (see the shim header for the full
// story).
#include "include/mm_audio_prefix.h"
#ifdef __cplusplus
#include <map>
#include <string>
#include <set>
#include <cstdint>

#ifdef RSBS_SINGLE_EXECUTABLE
// SoH declares byte-for-byte identically mangled AudioCollection /
// SequenceInfo / SeqType (soh/Enhancements/audio/AudioCollection.h) with a
// DIFFERENT class layout, so first-wins link resolution made both games share
// one implementation and one Instance slot. Namespace S2H keeps every method,
// static member and std::set instantiation distinct at link time; the
// using-declarations after the namespace keep unqualified upstream callers —
// including the out-of-class member definitions in AudioCollection.cpp —
// compiling unchanged.
namespace S2H {
#endif

enum SeqType {
    SEQ_NOSHUFFLE = 0,
    SEQ_BGM_WORLD = 1 << 0,
    SEQ_BGM_EVENT = 1 << 1,
    SEQ_BGM_BATTLE = 1 << 2,
    SEQ_OCARINA = 1 << 3,
    SEQ_FANFARE = 1 << 4,
    SEQ_BGM_ERROR = 1 << 5,
    SEQ_SFX = 1 << 6,
    SEQ_INSTRUMENT = 1 << 7,
    SEQ_VOICE = 1 << 8,
    SEQ_BGM_SONGS = 1 << 9,
    SEQ_BGM_CUSTOM = SEQ_BGM_WORLD | SEQ_BGM_EVENT | SEQ_BGM_BATTLE,
    SEQ_BGM_CUSTOM_FANFARE = SEQ_FANFARE | SEQ_OCARINA | SEQ_BGM_SONGS,
};

#define INSTRUMENT_OFFSET 0x81

struct SequenceInfo {
    uint16_t sequenceId;
    std::string label;
    std::string sfxKey;
    SeqType category;
    bool canBeReplaced;
    bool canBeUsedAsReplacement;
};

class AudioCollection {
  private:
    // All Loaded Audio
    std::map<uint16_t, SequenceInfo> mSequenceMap;

    // Sequences/SFX to include in/exclude from shuffle pool
    struct compareSequenceLabel {
        bool operator()(SequenceInfo* a, SequenceInfo* b) const {
            return a->label < b->label;
        };
    };
    std::set<SequenceInfo*, compareSequenceLabel> includedSequences;
    std::set<SequenceInfo*, compareSequenceLabel> excludedSequences;
    bool shufflePoolInitialized = false;

    std::map<SeqType, size_t> mSequenceTypeCounts;

  public:
    static AudioCollection* Instance;
    AudioCollection();
    std::map<uint16_t, SequenceInfo> GetAllSequences() const {
        return mSequenceMap;
    }
    std::set<SequenceInfo*, compareSequenceLabel> GetIncludedSequences() const {
        return includedSequences;
    };
    std::set<SequenceInfo*, compareSequenceLabel> GetExcludedSequences() const {
        return excludedSequences;
    };
    void AddToShufflePool(SequenceInfo*);
    void RemoveFromShufflePool(SequenceInfo*);
    void AddToCollection(char* otrPath, uint16_t seqNum);
    uint16_t GetReplacementSequence(uint16_t seqId);
    uint16_t GetOriginalSequence(uint16_t seqId);
    void InitializeShufflePool();
    const char* GetSequenceName(uint16_t seqId);
    bool HasSequenceNum(uint16_t seqId);
    size_t SequenceMapSize();
    std::string GetCvarKey(std::string sfxKey);
    std::string GetCvarLockKey(std::string sfxKey);
    size_t CountSequencesByType(SeqType type);
    uint16_t GetMaxOriginalSeqId() const;
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H
using S2H::AudioCollection;
using S2H::SeqType;
using S2H::SequenceInfo;
using enum S2H::SeqType;
#endif
#else
void AudioCollection_AddToCollection(char* otrPath, uint16_t seqNum);
const char* AudioCollection_GetSequenceName(uint16_t seqId);
bool AudioCollection_HasSequenceNum(uint16_t seqId);
size_t AudioCollection_SequenceMapSize();
#endif