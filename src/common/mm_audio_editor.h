#pragma once
/**
 * W6-AUDIO-SEQ (#1239): the seam between SoH's Audio Editor window, which draws Majora's Mask's music pane, and
 * Majora's Mask's sequence collection, which owns MM's sequences and their replacement keys.
 *
 * SoH's window cannot include MM's audio headers (both games declare an AudioCollection, a SequenceInfo and a SeqType,
 * kept apart only by MM's S2H namespace), and games/oot may not spell an MM key. So the pane reads plain copies of MM's
 * rows and calls these functions for every write. C++ only.
 */
#ifdef __cplusplus

/** UI snapshot harness only: on the Audio Editor's next frame, bring its Majora's Mask tab to the front (true) or its
 *  first tab, Audio Options (false). Defined beside the pane in games/oot/soh/Enhancements/audio/AudioEditor.cpp. */
void SohAudioEditor_ShowMmPaneForTest(bool show);

#endif
