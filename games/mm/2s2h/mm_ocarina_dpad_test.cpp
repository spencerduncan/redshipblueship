/**
 * @file mm_ocarina_dpad_test.cpp
 * ROM-free, display-free guard for the D-pad ocarina in Termina (#1062, fixed by
 * #1099; #1070's 2026-10-21 report). CTest label "redship", row
 * MMOcarinaDpadNotes in CMake/SingleExecutable.cmake (beside
 * MMCrossingSongPlayable), dispatch "mm-ocarina-dpad-notes" in
 * src/common/test_runner.cpp.
 *
 * WHY. On v0.2.0-prealpha the D-pad was dead in all four directions on MM's
 * ocarina: GameInteractor_Dpad(GI_DPAD_OCARINA) read MM's own
 * gEnhancements.Playback.DpadOcarina, which nothing in the combo writes, while
 * Settings > Controls > Ocarina Controls writes gSettings.CustomOcarina.Dpad.
 * #1099 pointed it at the SoH key (GameExports_SingleExe.cpp), but was locked
 * only by key-list rows; nothing fed a D-pad bit through MM's note path. This
 * row does, with the real pieces: the pad read goes through one fenced test
 * seam (MM_AudioOcarina_SetInputOverrideForTest in src/audio/code_8019AF00.c,
 * because a ROM-free process has no pad), and every frame is the real
 * AudioOcarina_Update: PlayControllerInput's five-arm note chain, then
 * CheckSongsWithoutMusicStaff (free play, the L rule), then the drop-input
 * timer, then UpdatePlayingStaff.
 *
 * A green guard, not a RED lock: no defect is open on main. With
 * GameInteractor_Dpad pointed back at gEnhancements.Playback.DpadOcarina (the
 * v0.2.0 key) legs 1 and 2 fail with pitch NONE in all four directions.
 *
 * LEGS.
 *   1. Note map: each D-pad direction, as a fresh press, plays the pitch and
 *      staff button of its C-button twin (Up D5 C-Up, Left B4 C-Left, Right A4
 *      C-Right, Down F4 C-Down) and advances the staff by exactly one; A is the
 *      control.
 *   2. Songs on the D-pad, in free play (AudioOcarina_StartDefault with the
 *      0xC0000000 flags z_message.c uses): Sonata, Elegy, Song of Time and
 *      Soaring, notes from gOcarinaSongButtons, each answered with its song
 *      index on the frame of the last note. With no song enabled the same
 *      notes advance the staff and the state stays 0xFE.
 *   3. Non-vacuity: with gSettings.CustomOcarina.Dpad cleared the D-pad plays
 *      nothing and leaves the staff alone while the C buttons still play.
 *   4. The L rule (CheckSongsWithoutMusicStaff): L+D-Up mid-song restarts the
 *      staff and the song then fails; L alone does not; with the key cleared
 *      L+D-Up does not.
 *   5. gEnhancements.DpadEquips=1 leaves legs 1 and 2 unchanged (nothing on
 *      the audio path reads it).
 *
 * WHAT IT CANNOT SEE: the player's deck bindings (one deck for both games) and
 * the message layer that draws the staff (z_message.c).
 *
 * Every CVar it touches is restored clear-never-store, the ocarina state is
 * snapshotted and restored, and the override is withdrawn, pass or fail.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The seam is single-exe only; test_runner.cpp declares this unconditionally.
extern "C" int MM_OcarinaDpad_RunHeadless(void) {
    printf("[TEST] mm-ocarina-dpad-notes: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <climits>
#include <cstring>

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
// src/audio/code_8019AF00.c. The state is file scope with external linkage;
// the names are the ones build-cmake/redship.map shows.
extern u32 sOcarinaInputButtonCur;
extern u32 sOcarinaInputButtonStart;
extern u32 sOcarinaInputButtonPrev;
extern s32 sOcarinaInputButtonPress;
extern u8 sCurOcarinaPitch;
extern u8 sPrevOcarinaPitch;
extern u8 sCurOcarinaButtonIndex;
extern u32 sOcarinaFlags;
extern u8 sIsOcarinaInputEnabled;
extern s8 sOcarinaInstrumentId;
extern u8 sOcarinaStaffPlayingPos;
extern u8 MM_sOcarinaDropInputTimer;
extern OcarinaStaff MM_sPlayingStaff;
extern OcarinaSongButtons gOcarinaSongButtons[OCARINA_SONG_MAX];

void AudioOcarina_Update(void);
void AudioOcarina_StartDefault(u32 ocarinaFlags);
void AudioOcarina_SetInstrument(u8 ocarinaInstrumentId);
// The fenced test seam (RSBS_SINGLE_EXECUTABLE) in AudioOcarina_ReadControllerInput.
void MM_AudioOcarina_SetInputOverrideForTest(const Input* input);
}

namespace {

constexpr const char* kTag = "mm-ocarina-dpad-notes";
constexpr const char* kDpadKey = "gSettings.CustomOcarina.Dpad";
constexpr const char* kRightStickKey = "gSettings.CustomOcarina.RightStick";
constexpr const char* kNoDropKey = "gEnhancements.DpadNoDropOcarinaInput";
constexpr const char* kDpadEquipsKey = "gEnhancements.DpadEquips";

// Free play as z_message.c starts it: 0xC0000000 marks "no music staff".
constexpr u32 kFreePlay = 0xC0000000u;
constexpr u32 kSongMask =
    (1u << OCARINA_SONG_SONATA) | (1u << OCARINA_SONG_ELEGY) | (1u << OCARINA_SONG_TIME) | (1u << OCARINA_SONG_SOARING);
constexpr u8 kStateNoSong = 0xFE;

// Held and released for 3 frames each, so the one-frame drop-input timer
// (AudioOcarina_Update sets it on every pitch change) never eats a press.
constexpr int kHoldFrames = 3;
constexpr int kReleaseFrames = 3;

Input sInput;

/** One real AudioOcarina_Update with @p buttons held. */
void Frame(u16 buttons) {
    memset(&sInput, 0, sizeof(sInput));
    sInput.cur.button = buttons;
    AudioOcarina_Update();
}

struct PressResult {
    u8 pitch;       // sCurOcarinaPitch on the press frame
    u8 buttonIndex; // sCurOcarinaButtonIndex on the press frame
    OcarinaStaff staff;
};

/** Holds @p buttons for kHoldFrames and releases for kReleaseFrames; returns the
 *  press frame's note and staff (the frame a song is matched on). */
PressResult Press(u16 buttons) {
    PressResult r;
    Frame(buttons);
    r.pitch = sCurOcarinaPitch;
    r.buttonIndex = sCurOcarinaButtonIndex;
    r.staff = MM_sPlayingStaff;
    for (int i = 1; i < kHoldFrames; i++) {
        Frame(buttons);
    }
    for (int i = 0; i < kReleaseFrames; i++) {
        Frame(0);
    }
    return r;
}

void StartFreePlay(u32 songMask) {
    MM_sOcarinaDropInputTimer = 0;
    AudioOcarina_StartDefault(songMask | kFreePlay);
}

int Fail(int code, const char* msg) {
    printf("[TEST] FAIL(%d): %s: %s\n", code, kTag, msg);
    return code;
}

struct NoteRow {
    const char* name;
    u16 dpad;
    u16 cButton;
    u8 pitch;
    u8 buttonIndex;
};

// The four arms of PlayControllerInput's note chain that take a D-pad term.
const NoteRow kNotes[] = {
    { "Down", BTN_DDOWN, BTN_CDOWN, OCARINA_PITCH_F4, OCARINA_BTN_C_DOWN },
    { "Right", BTN_DRIGHT, BTN_CRIGHT, OCARINA_PITCH_A4, OCARINA_BTN_C_RIGHT },
    { "Left", BTN_DLEFT, BTN_CLEFT, OCARINA_PITCH_B4, OCARINA_BTN_C_LEFT },
    { "Up", BTN_DUP, BTN_CUP, OCARINA_PITCH_D5, OCARINA_BTN_C_UP },
};

/** The pad button that plays staff button @p buttonIndex, on the D-pad when
 *  @p dpad (A has no D-pad twin). */
u16 PadFor(u8 buttonIndex, bool dpad) {
    switch (buttonIndex) {
        case OCARINA_BTN_A:
            return BTN_A;
        case OCARINA_BTN_C_DOWN:
            return dpad ? BTN_DDOWN : BTN_CDOWN;
        case OCARINA_BTN_C_RIGHT:
            return dpad ? BTN_DRIGHT : BTN_CRIGHT;
        case OCARINA_BTN_C_LEFT:
            return dpad ? BTN_DLEFT : BTN_CLEFT;
        case OCARINA_BTN_C_UP:
            return dpad ? BTN_DUP : BTN_CUP;
        default:
            return 0;
    }
}

struct SongRow {
    const char* name;
    u8 songIndex;
    int numNotes;
    u8 notes[8]; // staff buttons, from MM's song table
};

// MM's song table (gOcarinaSongButtons in code_8019AF00.c), not #1070's notes.
const SongRow kSongs[] = {
    { "Sonata of Awakening",
      OCARINA_SONG_SONATA,
      7,
      { OCARINA_BTN_C_UP, OCARINA_BTN_C_LEFT, OCARINA_BTN_C_UP, OCARINA_BTN_C_LEFT, OCARINA_BTN_A, OCARINA_BTN_C_RIGHT,
        OCARINA_BTN_A } },
    { "Elegy of Emptiness",
      OCARINA_SONG_ELEGY,
      7,
      { OCARINA_BTN_C_RIGHT, OCARINA_BTN_C_LEFT, OCARINA_BTN_C_RIGHT, OCARINA_BTN_C_DOWN, OCARINA_BTN_C_RIGHT,
        OCARINA_BTN_C_UP, OCARINA_BTN_C_LEFT } },
    { "Song of Time",
      OCARINA_SONG_TIME,
      6,
      { OCARINA_BTN_C_RIGHT, OCARINA_BTN_A, OCARINA_BTN_C_DOWN, OCARINA_BTN_C_RIGHT, OCARINA_BTN_A,
        OCARINA_BTN_C_DOWN } },
    { "Song of Soaring",
      OCARINA_SONG_SOARING,
      6,
      { OCARINA_BTN_C_DOWN, OCARINA_BTN_C_LEFT, OCARINA_BTN_C_UP, OCARINA_BTN_C_DOWN, OCARINA_BTN_C_LEFT,
        OCARINA_BTN_C_UP } },
};

// ---- Leg 0: the song table this row plays from --------------------------------
int CheckSongTable() {
    for (const auto& song : kSongs) {
        const OcarinaSongButtons& table = gOcarinaSongButtons[song.songIndex];
        bool same = table.numButtons == song.numNotes;
        for (int i = 0; same && i < song.numNotes; i++) {
            same = table.buttonIndex[i] == song.notes[i];
        }
        if (!same) {
            printf("[TEST] %s: gOcarinaSongButtons[%u] (%s) no longer holds the notes this row plays\n", kTag,
                   (unsigned)song.songIndex, song.name);
            return Fail(1, "MM's song table changed under the row; update kSongs from the table");
        }
    }
    return 0;
}

// ---- Leg 1: the note map ----------------------------------------------------
int CheckNoteMap(const char* phase) {
    // A, the control: no D-pad term in its arm.
    StartFreePlay(0);
    const PressResult a = Press(BTN_A);
    if (a.pitch != OCARINA_PITCH_D4 || a.buttonIndex != OCARINA_BTN_A || a.staff.pos != 1) {
        printf("[TEST] %s (%s): A gave pitch 0x%02X button %u pos %u\n", kTag, phase, a.pitch, a.buttonIndex,
               a.staff.pos);
        return Fail(2, "the A button does not play D4 on a fresh free-play staff; the harness is not driving the "
                       "real note path");
    }
    for (const auto& note : kNotes) {
        StartFreePlay(0);
        const PressResult c = Press(note.cButton);
        StartFreePlay(0);
        const PressResult d = Press(note.dpad);
        printf("[TEST] %s (%s): C-%s pitch 0x%02X button %u pos %u; D-%s pitch 0x%02X button %u pos %u\n", kTag, phase,
               note.name, c.pitch, c.buttonIndex, c.staff.pos, note.name, d.pitch, d.buttonIndex, d.staff.pos);
        if (c.pitch != note.pitch || c.buttonIndex != note.buttonIndex || c.staff.buttonIndex != note.buttonIndex ||
            c.staff.pos != 1) {
            return Fail(2, "a C button does not play its own note; the twin the D-pad is compared with is wrong");
        }
        if (d.pitch == OCARINA_PITCH_NONE) {
            return Fail(3, "a D-pad press played no note (pitch NONE): GameInteractor_Dpad(GI_DPAD_OCARINA) did not "
                           "open the D-pad terms with gSettings.CustomOcarina.Dpad on (#1062)");
        }
        if (d.pitch != c.pitch || d.buttonIndex != c.buttonIndex || d.staff.buttonIndex != c.staff.buttonIndex) {
            return Fail(3, "a D-pad direction plays a different note from its C-button twin");
        }
        if (d.staff.pos != 1) {
            return Fail(3, "a D-pad note did not advance the free-play staff by exactly one");
        }
    }
    return 0;
}

/** Plays @p song on the pad (the D-pad when @p dpad); returns the state on the
 *  last note's press frame, or -1 if an earlier note already ended the staff. */
int PlaySong(const SongRow& song, bool dpad, int* finalPos) {
    for (int i = 0; i < song.numNotes; i++) {
        const PressResult r = Press(PadFor(song.notes[i], dpad));
        if (finalPos != nullptr) {
            *finalPos = r.staff.pos;
        }
        if (i == song.numNotes - 1) {
            return r.staff.state;
        }
        if (r.staff.state != kStateNoSong) {
            printf("[TEST] %s: %s note %d answered state 0x%02X before the last note\n", kTag, song.name, i + 1,
                   r.staff.state);
            return -1;
        }
    }
    return -1;
}

// ---- Leg 2: songs on the D-pad ----------------------------------------------
int CheckSongs(const char* phase) {
    for (const auto& song : kSongs) {
        StartFreePlay(kSongMask);
        const int state = PlaySong(song, /*dpad=*/true, nullptr);
        printf("[TEST] %s (%s): %s on the D-pad -> state %d (expected %u)\n", kTag, phase, song.name, state,
               (unsigned)song.songIndex);
        if (state != song.songIndex) {
            return Fail(4, "a song played on the D-pad in free play was not recognised");
        }
    }
    // No song enabled: the staff still takes every note, and nothing matches.
    for (const auto& song : kSongs) {
        StartFreePlay(0);
        int pos = -1;
        const int state = PlaySong(song, /*dpad=*/true, &pos);
        if (state != kStateNoSong || pos != song.numNotes) {
            printf("[TEST] %s (%s): %s with no song enabled -> state %d pos %d\n", kTag, phase, song.name, state, pos);
            return Fail(5, "with no song enabled, the D-pad notes did not each advance the staff, or a song "
                           "matched anyway");
        }
    }
    return 0;
}

// ---- Leg 3: the key is the gate ---------------------------------------------
int CheckKeyOff() {
    CVarClear(kDpadKey);
    for (const auto& note : kNotes) {
        StartFreePlay(kSongMask);
        const u8 staffButtonBefore = MM_sPlayingStaff.buttonIndex;
        const PressResult d = Press(note.dpad);
        if (d.pitch != OCARINA_PITCH_NONE || d.staff.pos != 0 || d.staff.buttonIndex != staffButtonBefore) {
            printf("[TEST] %s: key off, D-%s gave pitch 0x%02X pos %u\n", kTag, note.name, d.pitch, d.staff.pos);
            return Fail(6, "with gSettings.CustomOcarina.Dpad cleared the D-pad still plays: the key is not the gate "
                           "and legs 1-2 prove nothing");
        }
        const PressResult c = Press(note.cButton);
        if (c.pitch != note.pitch || c.staff.pos != 1) {
            return Fail(6, "with the D-pad key cleared the C buttons stopped playing");
        }
    }
    StartFreePlay(kSongMask);
    if (PlaySong(kSongs[0], /*dpad=*/true, nullptr) == kSongs[0].songIndex) {
        return Fail(6, "with the D-pad key cleared the Sonata on the D-pad was still recognised");
    }
    return 0;
}

// ---- Leg 4: the L rule ------------------------------------------------------
/** Sonata's first three notes, then @p interjection held mid-song, then the
 *  last four. Returns the staff pos right after the interjection's press frame
 *  in *posAfter and the last note's state. */
int SonataWithInterjection(bool dpad, u16 interjection, int* posAfter) {
    const SongRow& sonata = kSongs[0];
    StartFreePlay(kSongMask);
    for (int i = 0; i < 3; i++) {
        Press(PadFor(sonata.notes[i], dpad));
    }
    *posAfter = Press(interjection).staff.pos;
    int state = -1;
    for (int i = 3; i < sonata.numNotes; i++) {
        state = Press(PadFor(sonata.notes[i], dpad)).staff.state;
    }
    return state;
}

int CheckLRule() {
    int pos = -1;
    int state = SonataWithInterjection(/*dpad=*/true, BTN_L | BTN_DUP, &pos);
    printf("[TEST] %s: key on, L+D-Up after three Sonata notes -> pos %d, Sonata state %d\n", kTag, pos, state);
    if (pos != 0) {
        return Fail(7, "L+D-Up mid-song did not restart the free-play staff (CheckSongsWithoutMusicStaff's L rule "
                       "lost its D-pad term)");
    }
    if (state == OCARINA_SONG_SONATA) {
        return Fail(7, "the Sonata still matched after L+D-Up restarted the staff");
    }

    state = SonataWithInterjection(/*dpad=*/true, BTN_L, &pos);
    printf("[TEST] %s: key on, L alone after three Sonata notes -> pos %d, Sonata state %d\n", kTag, pos, state);
    if (pos != 3 || state != OCARINA_SONG_SONATA) {
        return Fail(8, "L alone mid-song disturbed the staff; only L with a note button restarts it");
    }

    CVarClear(kDpadKey);
    state = SonataWithInterjection(/*dpad=*/false, BTN_L | BTN_DUP, &pos);
    CVarSetInteger(kDpadKey, 1);
    printf("[TEST] %s: key off, L+D-Up after three Sonata notes (C buttons) -> pos %d, Sonata state %d\n", kTag, pos,
           state);
    if (pos != 3 || state != OCARINA_SONG_SONATA) {
        return Fail(9, "with the D-pad key cleared L+D-Up still restarted the staff");
    }
    return 0;
}

int RunLegs() {
    int rc = CheckSongTable();
    if (rc != 0) {
        return rc;
    }
    if ((rc = CheckNoteMap("D-pad key on")) != 0) {
        return rc;
    }
    printf("[TEST] %s: leg 1: each D-pad direction plays its C-button twin's note and advances the staff\n", kTag);
    if ((rc = CheckSongs("D-pad key on")) != 0) {
        return rc;
    }
    printf("[TEST] %s: leg 2: Sonata, Elegy, Song of Time and Soaring are recognised on the D-pad\n", kTag);
    if ((rc = CheckKeyOff()) != 0) {
        return rc;
    }
    CVarSetInteger(kDpadKey, 1);
    printf("[TEST] %s: leg 3: with the key cleared the D-pad is inert and the C buttons still play\n", kTag);
    if ((rc = CheckLRule()) != 0) {
        return rc;
    }
    printf("[TEST] %s: leg 4: L+D-Up restarts the staff, L alone does not, and not with the key cleared\n", kTag);
    CVarSetInteger(kDpadEquipsKey, 1);
    if ((rc = CheckNoteMap("DpadEquips on")) != 0 || (rc = CheckSongs("DpadEquips on")) != 0) {
        return rc;
    }
    printf("[TEST] %s: leg 5: gEnhancements.DpadEquips leaves the note map and the songs unchanged\n", kTag);
    return 0;
}

struct SavedCVar {
    const char* key;
    int value; // INT_MIN: absent
};

struct OcarinaSnapshot {
    u32 cur, start, prev;
    s32 press;
    u8 curPitch, prevPitch, curButton;
    u32 flags;
    u8 inputEnabled;
    s8 instrument;
    u8 staffPos;
    u8 dropTimer;
    OcarinaStaff playingStaff;
};

OcarinaSnapshot TakeSnapshot() {
    return OcarinaSnapshot{
        sOcarinaInputButtonCur, sOcarinaInputButtonStart, sOcarinaInputButtonPrev, sOcarinaInputButtonPress,
        sCurOcarinaPitch,       sPrevOcarinaPitch,        sCurOcarinaButtonIndex,  sOcarinaFlags,
        sIsOcarinaInputEnabled, sOcarinaInstrumentId,     sOcarinaStaffPlayingPos, MM_sOcarinaDropInputTimer,
        MM_sPlayingStaff
    };
}

void RestoreSnapshot(const OcarinaSnapshot& s) {
    sOcarinaInputButtonCur = s.cur;
    sOcarinaInputButtonStart = s.start;
    sOcarinaInputButtonPrev = s.prev;
    sOcarinaInputButtonPress = s.press;
    sCurOcarinaPitch = s.curPitch;
    sPrevOcarinaPitch = s.prevPitch;
    sCurOcarinaButtonIndex = s.curButton;
    sOcarinaFlags = s.flags;
    sIsOcarinaInputEnabled = s.inputEnabled;
    sOcarinaInstrumentId = s.instrument;
    sOcarinaStaffPlayingPos = s.staffPos;
    MM_sOcarinaDropInputTimer = s.dropTimer;
    MM_sPlayingStaff = s.playingStaff;
}

} // namespace

extern "C" int MM_OcarinaDpad_RunHeadless(void) {
    printf("[TEST] %s: the D-pad plays Majora's Mask's ocarina notes and songs with "
           "gSettings.CustomOcarina.Dpad on (#1062/#1099 guard)\n",
           kTag);

    SavedCVar saved[] = { { kDpadKey, 0 }, { kRightStickKey, 0 }, { kNoDropKey, 0 }, { kDpadEquipsKey, 0 } };
    for (auto& s : saved) {
        s.value = CVarGetInteger(s.key, INT_MIN);
    }
    const OcarinaSnapshot snapshot = TakeSnapshot();

    CVarSetInteger(kDpadKey, 1);
    CVarClear(kRightStickKey);
    CVarClear(kNoDropKey);
    CVarClear(kDpadEquipsKey);

    // A ROM-free process never boots MM's audio, so its sfx bank lists are
    // still zeroed bss: entry 0's `next` is 0, and AudioSfx_StopById (which
    // PlayControllerInput calls on every note release) would walk that
    // self-loop forever. Initialise the bookkeeping the way MM's audio reset
    // does, only when it never ran; once run, `next` is 0xFF or a live index.
    bool sfxListsUnset = false;
    for (int bank = 0; bank < (int)ARRAY_COUNT(gSfxBanks); bank++) {
        sfxListsUnset = sfxListsUnset || (gSfxBanks[bank][0].next == 0);
    }
    if (sfxListsUnset) {
        AudioSfx_Reset();
    }

    // Nothing held when the instrument comes out, so sOcarinaInputButtonStart
    // is 0 and no press is swallowed as the button that opened the ocarina.
    memset(&sInput, 0, sizeof(sInput));
    MM_AudioOcarina_SetInputOverrideForTest(&sInput);
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
    MM_sOcarinaDropInputTimer = 0;

    int rc = 0;
    if (sOcarinaInputButtonStart != 0 || sOcarinaInstrumentId == OCARINA_INSTRUMENT_OFF) {
        rc = Fail(1, "taking out the ocarina with nothing held did not leave the reader armed");
    } else {
        rc = RunLegs();
    }

    // ---- Cleanup (pass and fail) -------------------------------------------
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
    MM_AudioOcarina_SetInputOverrideForTest(nullptr);
    RestoreSnapshot(snapshot);
    for (const auto& s : saved) {
        if (s.value == INT_MIN) {
            CVarClear(s.key);
        } else {
            CVarSetInteger(s.key, s.value);
        }
    }

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] %s: PASS (D-pad notes match their C-button twins; four songs play on the D-pad; the key gates "
           "it; the L rule restarts on L+D-pad only)\n",
           kTag);
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
