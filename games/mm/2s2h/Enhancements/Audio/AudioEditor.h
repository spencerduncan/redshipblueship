#pragma once
#include "stdint.h"

#include "libultraship/libultra/types.h"
// Single-exe: renames the AudioEditor_* functions declared below (extern "C"
// and C++ alike) to MM_-prefixed symbols. Included here — not just via
// AudioCollection.h — so C TUs that only include this header get the renamed
// extern "C" declarations too.
#include "include/mm_audio_prefix.h"
#ifdef __cplusplus
#include <ship/window/gui/GuiWindow.h>
#include "AudioCollection.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#ifdef RSBS_SINGLE_EXECUTABLE
// Same collision story as AudioCollection.h: SoH's class AudioEditor mangles
// identically (DrawElement/InitElement/vtable), so MM's moves into S2H.
namespace S2H {
#endif

class AudioEditor : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void DrawElement() override;
    void InitElement() override;
    void UpdateElement() override{};
    ~AudioEditor(){};

  private:
    // void DrawPreviewButton(uint16_t sequenceId, std::string sfxKey, SeqType sequenceType);
    // void Draw_SfxTab(const std::string& tabId, SeqType type);
    uint16_t mPlayingSeq = 0;
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H
using S2H::AudioEditor;
#endif

void AudioEditor_RandomizeAll();
void AudioEditor_RandomizeGroup(SeqType group);
void AudioEditor_ResetAll();
void AudioEditor_ResetGroup(SeqType group);
void AudioEditor_LockAll();
void AudioEditor_UnlockAll();

extern "C" {
#endif

u16 AudioEditor_GetReplacementSeq(u16 seqId);
u16 AudioEditor_GetOriginalSeq(u16 seqId);

#ifdef __cplusplus
}
#endif
