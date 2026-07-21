/**
 * Cross-game layout fingerprint for Notification::Options (#427 item 1).
 *
 * In single-exe builds MM's 2s2h/BenGui/Notification.cpp is excluded
 * (games/mm/CMakeLists.txt drops 2s2h/BenGui/*.cpp), so every
 * `Notification::Emit(Options)` MM compiles — e.g. the rando pickup toast in
 * 2s2h/Rando/MiscBehavior/CheckQueue.cpp — binds OoT's
 * soh/Notification/Notification.cpp definition. That bind is deliberate and
 * desirable (one shared Gui overlay renders notifications over both games), but
 * it survives purely because the two ports' `namespace Notification { struct
 * Options }` are field-identical and `Notification::Emit(Options)` mangles the
 * same in both — a coincidence, not a contract. If either game's Options ever
 * gains/loses/reorders/retypes a field, OoT's Emit reads MM's differently-laid
 * out struct: silent corruption, with no link error to catch it (only one
 * Emit definition survives).
 *
 * This is the wire format for the mm-notification-binding lock
 * (games/mm/2s2h/mm_notification_binding_test.cpp, CTest label "redship"): each
 * game fills it from ITS OWN view of Notification::Options — measured with
 * pointer arithmetic on a real instance so it is well-defined regardless of
 * standard-layout status and needs no hardcoded, platform-specific byte
 * offsets — and the lock fails the moment the two fingerprints disagree. The
 * struct itself carries no std/ImGui types on purpose, so it is identical in
 * both TUs' ABI.
 */
#ifndef NOTIFICATION_LAYOUT_PROBE_H
#define NOTIFICATION_LAYOUT_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NotificationOptionsLayout {
    uint32_t structSize;
    uint32_t offId;
    uint32_t offItemIcon;
    uint32_t offPrefix;
    uint32_t offPrefixColor;
    uint32_t offMessage;
    uint32_t offMessageColor;
    uint32_t offSuffix;
    uint32_t offSuffixColor;
    uint32_t offRemainingTime;
    uint32_t offMute;
} NotificationOptionsLayout;

// soh/Notification/Notification.cpp — OoT's own view of Notification::Options,
// compiled with OoT's headers and production flags.
void OoT_NotificationOptionsLayout(NotificationOptionsLayout* out);

// games/mm/2s2h/mm_notification_binding_test.cpp — MM's own view, compiled with
// MM's headers (2s2h/BenGui/Notification.h).
void MM_NotificationOptionsLayout(NotificationOptionsLayout* out);

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATION_LAYOUT_PROBE_H
