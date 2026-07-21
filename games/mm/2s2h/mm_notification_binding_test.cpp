/**
 * ROM-free lock for the deliberate MM->OoT Notification::Emit cross-bind
 * (#427 item 1). CTest label "redship", row mm-notification-binding in
 * src/common/test_runner.cpp.
 *
 * What is bound: MM's 2s2h/BenGui/Notification.cpp is excluded from the
 * single-exe build (games/mm/CMakeLists.txt drops 2s2h/BenGui/*.cpp), so every
 * `Notification::Emit(Options)` that MM compiles — the Rando pickup toast in
 * 2s2h/Rando/MiscBehavior/CheckQueue.cpp — resolves to OoT's
 * soh/Notification/Notification.cpp definition. `Notification::Emit(Options)`
 * mangles identically in both ports and the two `namespace Notification {
 * struct Options }` are field-identical, so the call links and runs correctly:
 * one shared Gui overlay renders the toast over whichever game is active, which
 * is the desirable behavior.
 *
 * Why it is a landmine, not a contract: nothing but the field-identical
 * coincidence keeps it safe. If either port's Options gains, loses, reorders,
 * or retypes a field, OoT's Emit copies/reads MM's differently-laid-out struct
 * argument — silent memory corruption. There is NO link error to catch it:
 * exactly one Emit definition survives, so the linker has nothing to reject
 * (the same shape as the #382 FlagTable/culling class). The issue asks for a
 * field-layout static probe; this is it.
 *
 * This test lives in an MM translation unit (like mm_culling_test.cpp and
 * mm_gi_shim_test.cpp) so MM's 2s2h/BenGui/Notification.h — MM's own view of
 * Options — is compiled with MM's headers and flags, and never enters OoT's or
 * the common runner's TU. It is exposed through the C entry point
 * MM_NotificationBinding_RunHeadless().
 *
 * Two locks, in order of strength:
 *
 * 1. FIELD TYPES (compile-time, both views). The static_asserts below pin each
 *    Options field's type in MM's view; the twin set in
 *    soh/Notification/Notification.cpp pins OoT's. A retype in either port —
 *    even one applied to BOTH ports in lockstep, which the runtime compare
 *    (2) cannot see because it only checks that the two agree — fails that
 *    port's build. Taking &o.<field> below also makes a rename/removal a
 *    compile error in this TU.
 *
 * 2. CROSS-GAME LAYOUT EQUALITY (runtime, the real lock). MM's and OoT's
 *    Options fingerprints (sizeof + every member offset, each reported from its
 *    own port's TU) are compared field by field. For this bind they must be
 *    EQUAL — the inverse of the culling/gi-shim locks, where the two ports'
 *    layouts must DIFFER. Any divergence means OoT's Emit is now reading a
 *    struct laid out differently than MM passes it; the test fails loudly and
 *    names the field that drifted. Offsets are measured with pointer
 *    arithmetic on a real instance, so no platform-specific byte offset is
 *    hardcoded and the lock holds across MSVC / libstdc++ / libc++ ABIs.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/BenGui/Notification.h"
#include "notification_layout_probe.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

static_assert(std::is_same_v<decltype(Notification::Options::id), uint32_t>,
              "MM Notification::Options::id retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::itemIcon), const char*>,
              "MM Notification::Options::itemIcon retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::prefix), std::string>,
              "MM Notification::Options::prefix retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::prefixColor), ImVec4>,
              "MM Notification::Options::prefixColor retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::message), std::string>,
              "MM Notification::Options::message retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::messageColor), ImVec4>,
              "MM Notification::Options::messageColor retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::suffix), std::string>,
              "MM Notification::Options::suffix retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::suffixColor), ImVec4>,
              "MM Notification::Options::suffixColor retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::remainingTime), float>,
              "MM Notification::Options::remainingTime retyped — breaks the OoT Emit bind (#427)");
static_assert(std::is_same_v<decltype(Notification::Options::mute), bool>,
              "MM Notification::Options::mute retyped — breaks the OoT Emit bind (#427)");

extern "C" void MM_NotificationOptionsLayout(NotificationOptionsLayout* out) {
    Notification::Options o;
    const char* base = reinterpret_cast<const char*>(&o);
    out->structSize = static_cast<uint32_t>(sizeof(Notification::Options));
    out->offId = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.id) - base);
    out->offItemIcon = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.itemIcon) - base);
    out->offPrefix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.prefix) - base);
    out->offPrefixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.prefixColor) - base);
    out->offMessage = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.message) - base);
    out->offMessageColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.messageColor) - base);
    out->offSuffix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.suffix) - base);
    out->offSuffixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.suffixColor) - base);
    out->offRemainingTime = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.remainingTime) - base);
    out->offMute = static_cast<uint32_t>(reinterpret_cast<const char*>(&o.mute) - base);
}

namespace {

#define NOTIF_FIELD_CHECK(field)                                                                           \
    do {                                                                                                   \
        if (mm.field != oot.field) {                                                                       \
            printf("[TEST] FAIL: Notification::Options." #field " differs — MM %u, OoT %u (the MM->OoT " \
                   "Emit bind now corrupts; #427)\n",                                                      \
                   (unsigned)mm.field, (unsigned)oot.field);                                               \
            ok = false;                                                                                    \
        }                                                                                                  \
    } while (0)

} // namespace

extern "C" int MM_NotificationBinding_RunHeadless(void) {
    NotificationOptionsLayout mm;
    NotificationOptionsLayout oot;
    MM_NotificationOptionsLayout(&mm);
    OoT_NotificationOptionsLayout(&oot);

    printf("[TEST] Notification::Options sizeof — MM %u, OoT %u\n", (unsigned)mm.structSize, (unsigned)oot.structSize);

    bool ok = true;
    NOTIF_FIELD_CHECK(structSize);
    NOTIF_FIELD_CHECK(offId);
    NOTIF_FIELD_CHECK(offItemIcon);
    NOTIF_FIELD_CHECK(offPrefix);
    NOTIF_FIELD_CHECK(offPrefixColor);
    NOTIF_FIELD_CHECK(offMessage);
    NOTIF_FIELD_CHECK(offMessageColor);
    NOTIF_FIELD_CHECK(offSuffix);
    NOTIF_FIELD_CHECK(offSuffixColor);
    NOTIF_FIELD_CHECK(offRemainingTime);
    NOTIF_FIELD_CHECK(offMute);

    if (!ok) {
        return 1;
    }

    printf("[TEST] PASS: mm-notification-binding — MM and OoT Notification::Options are layout-identical, so MM's "
           "Notification::Emit safely binds OoT's implementation\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
