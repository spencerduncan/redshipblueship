/**
 * Cross-game layout fingerprint for Notification::Options (#427 item 1).
 *
 * The argument-passing coincidence this started as is retired: MM no longer
 * hands its own Options to OoT's Emit, it packs plain-C ComboNotification data
 * (src/common/notification_bridge.h) and OoT unpacks into its own Options by
 * field name. What that does NOT retire is the type itself. Both trees declare
 * `namespace Notification { struct Options }` with identical mangled names —
 * `TYPE Options` is entry 42 of .github/odr-declaration-baseline.txt — and the
 * struct is non-trivial (three std::string members), so each port's TU emits
 * COMDAT/linkonce copies of the same implicitly-defined special members
 * (`Options::Options()`, `Options::~Options()`). The linker keeps one and
 * discards the other. MM constructs and destroys Options in its own TUs (the
 * bridge, mm_notification_binding_test.cpp, AutoBankDeposit's `notif`, the
 * designated-initializer temporaries at every call site), so if the two views
 * ever stop being layout-identical, whichever copy survives runs against the
 * other port's objects: a destructor walking a std::string that is not there,
 * or a constructor writing past the end. No link error can catch it — COMDAT
 * folding is exactly the mechanism that makes duplicate definitions legal (the
 * #468/#470 class).
 *
 * So the two layouts must still be EQUAL, for a different reason than before,
 * and this stays the wire format for that half of the lock
 * (games/mm/2s2h/mm_notification_binding_test.cpp, CTest label "redship"): each
 * game fills it from ITS OWN view of Notification::Options — measured with
 * pointer arithmetic on a real instance so it is well-defined regardless of
 * standard-layout status and needs no hardcoded, platform-specific byte
 * offsets — and the lock fails the moment the two fingerprints disagree. The
 * struct itself carries no std/ImGui types on purpose, so it is identical in
 * both TUs' ABI.
 *
 * Retiring this lock needs the type names to stop colliding (an MM-side rename
 * or an enclosing namespace), not the bridge.
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
