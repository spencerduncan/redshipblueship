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
 * toast that leaves an 832-px window.
 *
 * Game-header-free C (ADR 0002): the spoiler terms are the MACHINE names the MM
 * spoiler loader reports (Rando/Spoiler/Apply.cpp), passed as strings.
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
    /** #610: a dropped MM spoiler's cross-game section names another world; `detail` is the term. */
    RSBS_PAIRING_REFUSAL_SPOILER = 3,
    /** PR #743 review: a paired world's OoT spoiler dropped at file select is not a solo OoT world. */
    RSBS_PAIRING_REFUSAL_OOT_SPOILER = 4,
    RSBS_PAIRING_REFUSAL_COUNT
} RsbsPairingRefusal;

/**
 * The longest RULES message, in characters. Set from the pixel lock: at 38
 * characters every one of the 8,191 field combinations leaves the toast at most
 * 775 px wide, inside the 802 px the overlay leaves in an 832-px window (the
 * window less its 30-px margin); at 39 one combination reaches 803 px.
 */
#define RSBS_PAIRING_REFUSAL_RULES_MAX_CHARS 38

/** The toast's prefix for @p kind; never NULL ("" for an unknown kind). */
const char* Combo_PairingRefusalToastPrefix(int kind);

/**
 * Write the toast's message for @p kind into @p out (always NUL-terminated when
 * @p len > 0).
 *
 * @param detail RULES: Combo_ComboSettingsDivergenceDescribe's ", "-separated
 *        field list; SPOILER: the diverged identity term's machine name; ignored
 *        by the other kinds. NULL or "" names nothing.
 * @return how many detail items the message names: RULES, the fields named in
 *         full (the rest are counted as "+N"); SPOILER, 1 when the term is one
 *         the copy knows; 0 otherwise.
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

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_PAIRING_REFUSAL_TOAST_H
