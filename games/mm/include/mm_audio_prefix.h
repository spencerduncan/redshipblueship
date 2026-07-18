#ifndef MM_AUDIO_PREFIX_H
#define MM_AUDIO_PREFIX_H

/**
 * Single-exe symbol-prefix shim for MM's audio-editor/audio-collection API.
 *
 * Both ports define an identically-named audio-editor family (SoH:
 * soh/Enhancements/audio/AudioCollection.{h,cpp} + AudioEditor.{h,cpp}; 2S2H:
 * 2s2h/Enhancements/Audio/ same names). The extern "C" wrappers below and the
 * C++ AudioEditor_* helpers collide by name, and the AudioCollection /
 * AudioEditor classes mangle identically member-for-member (both games even
 * share the std::set<SequenceInfo*, compareSequenceLabel> instantiations).
 * Under /FORCE:MULTIPLE (and first-wins archive resolution generally) one
 * game's implementation silently answers the other game's queries: MM's
 * sequence-replacement lookups executed SoH's code against OoT's sequence
 * table, and both games shared a single AudioCollection::Instance pointer
 * slot.
 *
 * Two mechanisms split the family, mirroring the repo-wide MM_ prefix
 * convention (only MM is renamed; SoH keeps the upstream names):
 *  - functions (this header): token-level renames, visible to every MM TU
 *    because AudioCollection.h / AudioEditor.h include this shim before the
 *    declarations — C callers in src/audio/ get the renamed extern "C"
 *    symbols without source changes;
 *  - types: AudioCollection.h / AudioEditor.h move the classes into
 *    namespace S2H (MM's established rename namespace, see 2s2h/resource/)
 *    under RSBS_SINGLE_EXECUTABLE, with using-declarations so unqualified
 *    upstream callers compile unchanged.
 *
 * MM's AudioEditor.cpp itself is excluded from single-exe builds (its UI
 * needs BenGui/BenMenu, which have not been ported); the two extern "C"
 * entry points below that MM's audio core calls are identity-stubbed in
 * 2s2h/GameExports_SingleExe.cpp instead.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

/* extern "C" wrappers, called from games/mm/src/audio/ C code */
#define AudioCollection_AddToCollection MM_AudioCollection_AddToCollection
#define AudioCollection_GetSequenceName MM_AudioCollection_GetSequenceName
#define AudioCollection_HasSequenceNum MM_AudioCollection_HasSequenceNum
#define AudioCollection_SequenceMapSize MM_AudioCollection_SequenceMapSize
#define AudioEditor_GetReplacementSeq MM_AudioEditor_GetReplacementSeq
#define AudioEditor_GetOriginalSeq MM_AudioEditor_GetOriginalSeq

/* C++ helpers declared in AudioEditor.h. Dormant in single-exe builds while
 * AudioEditor.cpp is excluded, but renamed now so re-enabling that file can
 * never resurrect a silent collision with SoH's identically-named set. */
#define AudioEditor_RandomizeAll MM_AudioEditor_RandomizeAll
#define AudioEditor_RandomizeGroup MM_AudioEditor_RandomizeGroup
#define AudioEditor_ResetAll MM_AudioEditor_ResetAll
#define AudioEditor_ResetGroup MM_AudioEditor_ResetGroup
#define AudioEditor_LockAll MM_AudioEditor_LockAll
#define AudioEditor_UnlockAll MM_AudioEditor_UnlockAll

/* AudioHook.cpp file-level helpers. Upstream leaves them non-static, and
 * SoH's AudioHooks.cpp defines the same names with the same signatures
 * (?NotifySequenceName@@YAXHH@Z showed up in the 2026-07-18 dumpbin
 * intersection). AudioHook.cpp sees this shim via its AudioCollection.h
 * include. */
#define NotifySequenceName MM_NotifySequenceName
#define RegisterAudioNotificationHooks MM_RegisterAudioNotificationHooks

#endif // RSBS_SINGLE_EXECUTABLE

#endif // MM_AUDIO_PREFIX_H
