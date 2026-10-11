/**
 * @file pairing_refusal_toast.h
 * @brief The player-visible copy of the cross-game refusal toasts: the class's
 *        one prefix, the refused file's words, the arrival's and the MM spoiler's
 *        refusals, and OoT's paired-spoiler refusal, in ONE place, so their
 *        production emitters, the ui tier's toast pages and the width lock all
 *        read the same strings.
 *
 * WHY THE COPY LIVES HERE. Every one of these toasts used to be a hand-written
 * sentence pair at its call site, 99 to 155 characters behind a 27-character
 * "Cross-game pairing REFUSED:" prefix. The shared notification overlay (SoH's
 * Notification::Window, games/oot/soh/Notification/Notification.cpp) draws the
 * prefix and the message on ONE line, in the default font (Montserrat 20,
 * OTRGlobals.cpp) at the player's Notifications.Size (default 1.8), and never
 * wraps, so the MM-options refusal drew a toast about 2,480 px wide: off the left
 * edge of any window narrower than about 2,510 px. PR #749 set the convention for
 * our toasts (docs/ui-style-guide.md section 10): SoH's default colours, the
 * player's configured duration, a short prefix and one short message that fits
 * the 832-px window the ui tier renders. These follow it.
 *
 * WHAT THE COPY KEEPS. The class in the prefix (RSBS_REFUSAL_TOAST_PREFIX,
 * "Refused:": each refusal latches the unified-save slot against writes for the
 * session, and the file, half or document is not applied) and the reason in the
 * message. A reason that NAMES something keeps naming it: the combo-record
 * refusal names the diverged fields while they fit (ADR 0011 decision 4,
 * Combo_ComboSettingsDivergenceDescribe) and counts the rest; the spoiler refusal
 * names which identity term diverged in words. The long explanation stays on each
 * refusal's stderr line, unchanged, with the machine names and the numbers.
 *
 * THE BUDGET is characters here and pixels in the lock. This file cannot measure
 * a glyph, so it bounds the one variable-length message by a character count; the
 * lock (pairing-refusal-toast-fit, games/oot/soh/soh_notification_fit_test.cpp)
 * draws every message through SoH's own overlay in the real font and fails any
 * toast that does not keep the overlay's 30-px margin on BOTH sides of an 832-px
 * window (at most 772 px wide) at the player's own size: the copy contract. The
 * real minimum window is libultraship's default 640 x 480 (#906), where the
 * overlay draws a toast whose copy is wider than the window a little smaller
 * (Notification::Window::Draw, FitToastToWindow); the lock draws that window too.
 *
 * THE RULES REFUSAL NAMES FIELDS BY THEIR RECORD NAMES ("goal", "direction",
 * "triforceHunt"), a recorded exception to R-N8 (docs/ui-style-guide.md section
 * 10): ADR 0011 decision 4 requires the refusal to name the field, and eleven
 * of the fourteen fields (poolSizeOoT and poolSizeMM since #801 retired their
 * rows, itemClassOoT and itemClassMM since #834 retired theirs, sharedOcarina
 * since #879 retired its row, logicRung, comboFlags, spare1, formatVersion,
 * comboSettingsHash, triforceHunt) have no row on that page to name; goal,
 * direction and sharedKokiriSword (#886) do.
 *
 * Game-header-free C (ADR 0002): the spoiler refusal's detail is one of the
 * RSBS_SPOILER_REFUSAL_* route keys below, which the MM spoiler loader
 * (Rando/Spoiler/Apply.cpp, ForeignIdentityDiverges) reports beside the machine
 * term it prints on stderr.
 */
#ifndef RSBS_COMMON_PAIRING_REFUSAL_TOAST_H
#define RSBS_COMMON_PAIRING_REFUSAL_TOAST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RsbsPairingRefusal {
    /** #570: the MM option profile resolved at the arrival is not the one frozen at creation. */
    RSBS_PAIRING_REFUSAL_MM_OPTIONS = 0,
    /** ADR 0011 decision 4: the combo record diverged; `detail` is the field list. */
    RSBS_PAIRING_REFUSAL_RULES = 1,
    /** ADR 0010 increment 2: the pairing identity has no MM half to hydrate. */
    RSBS_PAIRING_REFUSAL_MISSING_HALF = 2,
    /** #610: a dropped MM spoiler's cross-game section names another world; `detail` is the route key. */
    RSBS_PAIRING_REFUSAL_SPOILER = 3,
    /** PR #743 review: a paired world's OoT spoiler dropped at file select is not a solo OoT world. */
    RSBS_PAIRING_REFUSAL_OOT_SPOILER = 4,
    RSBS_PAIRING_REFUSAL_COUNT
} RsbsPairingRefusal;

/**
 * The longest RULES message, in characters. Set from the pixel lock: at 37
 * characters every one of the 8,191 field combinations leaves the toast inside
 * the 772 px the overlay's two 30-px margins leave in an 832-px window; at 38 one
 * combination reaches 775 px.
 */
#define RSBS_PAIRING_REFUSAL_RULES_MAX_CHARS 37

/*
 * The spoiler refusal's ROUTE KEYS: which check in ForeignIdentityDiverges
 * refused, not which identity term it names. One term can be reported by more
 * than one route (sharedRandoSeed is reported both when the spoiler names another
 * seed and when it names none), and the toast says what the route means, so the
 * copy is keyed on the route.
 */
/** The session is not playing a generated cross-game world. */
#define RSBS_SPOILER_REFUSAL_NOT_PAIRED "notPaired"
/** The session recorded no settings profile, so nothing can pair with it. */
#define RSBS_SPOILER_REFUSAL_SESSION_UNSETTLED "sessionHasNoSettings"
/** The spoiler carries no cross-game identity block. */
#define RSBS_SPOILER_REFUSAL_NO_IDENTITY "noIdentity"
/** The spoiler's identity block omits the seed or the settings digest. */
#define RSBS_SPOILER_REFUSAL_IDENTITY_INCOMPLETE "identityIncomplete"
/** The spoiler names another shared seed. */
#define RSBS_SPOILER_REFUSAL_OTHER_SEED "otherSeed"
/** The spoiler names another shared settings digest. */
#define RSBS_SPOILER_REFUSAL_OTHER_SETTINGS "otherSettings"
/** The spoiler names another frozen Majora's Mask option profile. */
#define RSBS_SPOILER_REFUSAL_OTHER_MM_OPTIONS "otherMmOptions"

/**
 * THE ONE PREFIX of every cross-game refusal toast (#836, operator ruling
 * 2026-10-01): the file select's and the load's refusals of a file, the MM
 * arrival's refusals of a half and the MM spoiler's refusal of a document. It
 * is the class's own name, and it is true at every site. Every message after it
 * is capitalized and has no trailing period, so the Combo > Save Files page's
 * status cell is this prefix and the toast's own words. The confirmations
 * ("Restored from file:", "Restored for Majora's Mask:") and the toasts outside
 * the class ("Spoiler not loaded:", "Not created:", "Not proven:", "Fewer
 * cross-game items:", "Save failed:") keep their own prefixes. The integration drives' toast
 * matchers read this constant, so they cannot go vacuous when the copy moves.
 */
#define RSBS_REFUSAL_TOAST_PREFIX "Refused:"

/**
 * The words a refused FILE is refused with (the file select's toast, the load's
 * toast and the Combo > Save Files status cell): one entry per distinct reason a
 * player reads, in one table (pairing_refusal_toast.c), so a page, a toast and
 * the width lock cannot drift apart. save.cpp maps its RsbsRefuseReason codes
 * and identity outcomes onto these.
 */
typedef enum RsbsRefusalWords {
    RSBS_REFUSAL_WORDS_UNREADABLE = 0,    // "File could not be read"
    RSBS_REFUSAL_WORDS_NOT_A_SAVE,        // "Not a save file"
    RSBS_REFUSAL_WORDS_OTHER_BUILD,       // "File made by another build" (version, tier size, other-build field)
    RSBS_REFUSAL_WORDS_WRONG_SLOT,        // "File belongs to another slot"
    RSBS_REFUSAL_WORDS_INCOMPLETE,        // "File is incomplete"
    RSBS_REFUSAL_WORDS_DAMAGED,           // "File is damaged"
    RSBS_REFUSAL_WORDS_RECORD_DAMAGED,    // "Cross-game record is damaged" (combo magic, identity damage)
    RSBS_REFUSAL_WORDS_OLDER_THAN_OOT,    // "Older than the Ocarina of Time save" (commit skew)
    RSBS_REFUSAL_WORDS_RULES_DIFFER,      // "Cross-game rules differ"
    RSBS_REFUSAL_WORDS_ITEMS_DAMAGED,     // "Cross-game items are damaged"
    RSBS_REFUSAL_WORDS_RECORD_MISSING,    // "Cross-game record is missing"
    RSBS_REFUSAL_WORDS_MM_OPTIONS_DIFFER, // "Majora's Mask options differ"
    RSBS_REFUSAL_WORDS_NO_MM_WORLD,       // "This file has no Majora's Mask world"
    RSBS_REFUSAL_WORDS_OTHER_FILE,        // "Cross-game record is for another file" (#564 V12: another OoT world)
    // The page's fallbacks for a refusal whose words were not recorded (every
    // refusal the file select posts records its words; these are reached only
    // through the session's record of a refusal that posted none).
    RSBS_REFUSAL_WORDS_SETTINGS_DIFFER, // "Settings differ from its creation" (identity)
    RSBS_REFUSAL_WORDS_NOT_GENERATED,   // "Termina could not be generated" (generation)
    RSBS_REFUSAL_WORDS_UNCHECKED,       // "File could not be checked" (no reason recorded)
    RSBS_REFUSAL_WORDS_COUNT
} RsbsRefusalWords;

/** The words for @p which (an RsbsRefusalWords); never NULL, "" past the table. */
const char* Combo_RefusalWords(int which);

/** The toast's prefix for @p kind; never NULL ("" for an unknown kind). */
const char* Combo_PairingRefusalToastPrefix(int kind);

/**
 * Write the toast's message for @p kind into @p out (always NUL-terminated when
 * @p len > 0).
 *
 * @param detail RULES: Combo_ComboSettingsDivergenceDescribe's ", "-separated
 *        field list; SPOILER: an RSBS_SPOILER_REFUSAL_* route key; ignored by
 *        the other kinds. NULL or "" names nothing.
 * @return how many detail items the message names: RULES, the fields named in
 *         full (the rest are counted as "+N"); SPOILER, 1 when the route key is
 *         one the copy knows; 0 otherwise.
 */
int Combo_PairingRefusalToastMessage(int kind, const char* detail, char* out, size_t len);

/**
 * THE MM EMITTER for the four refusal-class kinds (games/mm/2s2h/GameExports_SingleExe.cpp):
 * builds the copy above and queues it through MM's half of the notification
 * bridge with Notification::Options' defaults (SoH's colours, the player's
 * configured duration), muted because every caller can run without OoT's audio
 * session (MM's boot path, the display-free locks). Every production refusal site
 * calls it, and so do the ui tier's toast pages and the width lock, so what they
 * draw is what a player gets.
 */
void MM_Rando_EmitPairingRefusalToast(int kind, const char* detail);

/**
 * THE OoT EMITTER for RSBS_PAIRING_REFUSAL_OOT_SPOILER
 * (games/oot/soh/Enhancements/randomizer/SeedContext.cpp): builds the copy above
 * and queues it through SoH's Notification::Emit with Options' defaults. The
 * refusal site (Context::ParseSpoiler) calls it unmuted, as SoH's file-select
 * toasts are; the ui tier's toast page and the width lock call it muted, because
 * they run without OoT's audio session.
 */
void OoT_EmitPairedSpoilerRefusalToast(int mute);

/**
 * #564 V18: A SAVE THAT DID NOT REACH THE FILE. Every durable MM save route
 * (owl statue, pause save, Song of Time, game over, the two special saves, the
 * autosave) commits through MM_Combo_CaptureSaveToUnifiedSlot, and each one runs
 * its own success ceremony whatever the commit did. A commit that could not land
 * used to say so on stderr only, so the player saw the ceremony and kept nothing.
 * The capture now names the cause on the shared overlay, once per cause (a
 * periodic autosave or a new day into the same locked file does not repeat it);
 * a commit that lands, or another active slot, clears that record, so the next
 * failure is shown again.
 *
 * Outside the refusal class (this toast refuses nothing and latches nothing: it
 * reports what a latch, or the disk, already decided), so it has its own prefix,
 * and it never touches the Combo > Save Files page's refusal words: a refused
 * slot keeps the reason it was refused for. The prefix is not "Not saved:": that
 * was one of the refusal class's retired prefixes (#836), and
 * test_refusal_toasts_use_one_prefix keeps it retired. Capitalized, no trailing
 * period, like every message on the overlay; PairingRefusalToastFit draws each
 * one.
 */
typedef enum RsbsSaveFailure {
    /** No unified-save slot is active this session (a standalone MM session, or one with no file open). */
    RSBS_SAVE_FAILURE_NO_FILE = 0,
    /** The active slot is latched against writes this session (#533/#568): refused, or never opened. */
    RSBS_SAVE_FAILURE_LOCKED,
    /** The slot was writable but the commit did not land (staging refused, or the file write failed). */
    RSBS_SAVE_FAILURE_NOT_WRITTEN,
    RSBS_SAVE_FAILURE_COUNT
} RsbsSaveFailure;

/** The prefix of the save-failure toast. */
#define RSBS_SAVE_FAILURE_TOAST_PREFIX "Save failed:"

/** The message for @p cause (an RsbsSaveFailure); never NULL, "" past the table. */
const char* Combo_SaveFailureWords(int cause);

/**
 * THE MM EMITTER for a save that did not reach the file
 * (games/mm/2s2h/GameExports_SingleExe.cpp): RSBS_SAVE_FAILURE_TOAST_PREFIX and
 * Combo_SaveFailureWords(@p cause), in SoH's toast shape, muted (it runs on MM's
 * game thread, where OoT's audio session is not live). Unconditional: the
 * once-per-cause rule belongs to the capture that calls it.
 */
void MM_Combo_EmitSaveFailureToast(int cause);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_PAIRING_REFUSAL_TOAST_H
