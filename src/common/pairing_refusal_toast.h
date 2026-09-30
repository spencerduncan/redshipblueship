/**
 * @file pairing_refusal_toast.h
 * @brief The player-visible copy of the cross-game refusal toasts: the four
 *        "this session is not saved to the pair" refusals and OoT's paired-spoiler
 *        refusal, in ONE place, so their production emitters, the ui tier's toast
 *        pages and the width lock all read the same strings.
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
 * WHAT THE COPY KEEPS. The outcome in the prefix ("Not saved:": each refusal
 * latches the unified-save slot against writes for the session) and the reason
 * in the message. A reason that NAMES something keeps naming it: the combo-record
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
 * window (at most 772 px wide).
 *
 * THE RULES REFUSAL NAMES FIELDS BY THEIR RECORD NAMES ("goal", "itemClassMM",
 * "triforceHunt"), a recorded exception to R-N8 (docs/ui-style-guide.md section
 * 10): ADR 0011 decision 4 requires the refusal to name the field, the Cross-Game
 * Rules rows' own labels carry the same abbreviations ("OoT Classes", "MM
 * Classes"), and eight of the thirteen fields (poolSizeOoT and poolSizeMM since
 * #801 retired their rows, logicRung, comboFlags, spare1, formatVersion,
 * comboSettingsHash, triforceHunt) have no row on that page to name.
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
 * THE MM EMITTER for the four "Not saved:" kinds (games/mm/2s2h/GameExports_SingleExe.cpp):
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

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_PAIRING_REFUSAL_TOAST_H
