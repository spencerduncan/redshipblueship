/**
 * ROM-free lock for the MM->OoT notification bridge (#427 item 1). CTest label
 * "redship", row mm-notification-binding in src/common/test_runner.cpp.
 *
 * What is bound: MM's 2s2h/BenGui/Notification.cpp is excluded from the
 * single-exe build (games/mm/CMakeLists.txt drops 2s2h/BenGui/*.cpp), so MM has
 * no notification overlay of its own — MM's toasts (the Rando pickup toast in
 * 2s2h/Rando/MiscBehavior/CheckQueue.cpp, the sequence-name toast in
 * Enhancements/Audio/AudioHook.cpp, the bank-deposit toast in
 * Enhancements/Timesavers/AutoBankDeposit.cpp) have to render on OoT's. One
 * shared overlay over whichever game is active is the desirable behavior.
 *
 * How it used to get there was not: `Notification::Emit(Notification::Options)`
 * mangles identically in both ports, so MM's call bound OoT's surviving body
 * and handed it MM's own view of Options. Correct only while the two structs
 * stayed field-identical, and no link error can ever catch a divergence —
 * exactly one Emit definition survives, so the linker has nothing to reject
 * (the #382 FlagTable/culling class). Since #427 item 1 the coincidence is
 * retired: MM's single-exe Notification.h no longer declares `Emit` at all, and
 * MM packs plain-C ComboNotification data (src/common/notification_bridge.h)
 * through Notification::MM_Notify_Emit -> OoT_Notification_Emit instead.
 *
 * This test lives in an MM translation unit (like mm_culling_test.cpp and
 * mm_gi_shim_test.cpp) so it drives the bridge from MM's side, with MM's own
 * headers and flags — the exact path a real MM toast takes. It is exposed
 * through the C entry point MM_NotificationBinding_RunHeadless().
 *
 * Three locks, listed by strength; (2) runs first at runtime for the reason
 * given at its call site:
 *
 * 1. END-TO-END PAYLOAD (runtime, the real lock). Two MM-built
 *    Notification::Options go through MM_Notify_Emit; each is then read back
 *    out of OoT's overlay store and compared field by field. This asserts the
 *    ground truth — an MM toast arrives at OoT's overlay with every field
 *    intact and in the right place — rather than a derived count. It goes red
 *    if the bridge is reverted (this TU stops compiling, because MM has no
 *    `Notification::Emit` to call), if either half drops or crosses a field, or
 *    if the second emit does not land after the first (proving the toast is
 *    appended by THIS call, not read as a leftover).
 *
 *    Kept headless-safe deliberately: mute = true and a non-zero remainingTime
 *    are the two inputs that keep OoT's Emit off both CVar reads and
 *    Audio_PlaySoundGeneral, so the row needs no window, no CVar store and no
 *    audio context.
 *
 * 2. CROSS-GAME LAYOUT EQUALITY (runtime). The bridge stopped MM's Options from
 *    being PASSED to a body compiled against OoT's view; it did not stop the
 *    two ports from declaring the same `Notification::Options` type name (entry
 *    `TYPE Options` of .github/odr-declaration-baseline.txt). The struct is
 *    non-trivial, so the implicitly-defined `Options::Options()` and
 *    `Options::~Options()` that MM's TUs emit are COMDAT duplicates of OoT's
 *    and the linker folds them to one. MM builds and destroys Options in MM
 *    TUs, so the two layouts must still be EQUAL or the surviving copy walks
 *    the wrong offsets — and COMDAT folding is precisely the mechanism that
 *    makes that legal, so no link error can report it. The two fingerprints
 *    (sizeof + every member offset, each reported from its own port's TU) are
 *    compared field by field, measured with pointer arithmetic on a real
 *    instance so no platform-specific byte offset is hardcoded.
 *
 * 3. FIELD TYPES (compile-time, both views). MM's Options is checked against
 *    the shared COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT list — the same one
 *    soh/Notification/Notification.cpp compiles against OoT's Options and
 *    games/mm/2s2h/mm_notification_bridge.cpp against MM's. A retype in either
 *    port fails that port's build, including a lockstep retype of both, which
 *    no runtime comparison could see.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/BenGui/Notification.h"
#include "notification_bridge.h"
#include "notification_layout_probe.h"

#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <type_traits>

COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT(Notification::Options);

extern "C" void MM_NotificationOptionsLayout(NotificationOptionsLayout* out) {
    // Slack buffer + placement new, never destroyed. If the two views have
    // already diverged then the implicit constructor the linker folded may be
    // the OTHER port's and may write past this port's sizeof; a plain local
    // fail-fasts on the stack cookie before the fingerprint can be reported
    // (measured on MSVC), which costs exactly the diagnostic this row exists to
    // produce. The 4x is slack, not a bound, and skipping the destructor keeps
    // an over-reading folded dtor out of the picture too.
    alignas(alignof(Notification::Options)) static unsigned char storage[sizeof(Notification::Options) * 4] = {};
    const Notification::Options* o = new (storage) Notification::Options();

    const char* base = reinterpret_cast<const char*>(o);
    out->structSize = static_cast<uint32_t>(sizeof(Notification::Options));
    out->offId = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->id) - base);
    out->offItemIcon = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->itemIcon) - base);
    out->offPrefix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->prefix) - base);
    out->offPrefixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->prefixColor) - base);
    out->offMessage = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->message) - base);
    out->offMessageColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->messageColor) - base);
    out->offSuffix = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->suffix) - base);
    out->offSuffixColor = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->suffixColor) - base);
    out->offRemainingTime = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->remainingTime) - base);
    out->offMute = static_cast<uint32_t>(reinterpret_cast<const char*>(&o->mute) - base);
}

namespace {

#define NOTIF_FIELD_CHECK(field)                                                                                 \
    do {                                                                                                         \
        if (mm.field != oot.field) {                                                                             \
            printf("[TEST] FAIL: Notification::Options." #field " differs - MM %u, OoT %u; the folded implicit " \
                   "ctor/dtor now walks the wrong offsets (#427)\n",                                             \
                   (unsigned)mm.field, (unsigned)oot.field);                                                     \
            ok = false;                                                                                          \
        }                                                                                                        \
    } while (0)

bool LayoutsAgree(void) {
    NotificationOptionsLayout mm;
    NotificationOptionsLayout oot;
    MM_NotificationOptionsLayout(&mm);
    OoT_NotificationOptionsLayout(&oot);

    printf("[TEST] Notification::Options sizeof - MM %u, OoT %u\n", (unsigned)mm.structSize, (unsigned)oot.structSize);

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
    return ok;
}

// Every literal below is exactly representable in float, so the comparison is
// an exact equality on the bits the bridge copied, not a tolerance check.
bool ColorLanded(const char* field, const float landed[4], const ImVec4& sent) {
    if (landed[0] == sent.x && landed[1] == sent.y && landed[2] == sent.z && landed[3] == sent.w) {
        return true;
    }
    printf("[TEST] FAIL: Notification %s crossed wrong - sent (%f %f %f %f), landed (%f %f %f %f) (#427)\n", field,
           (double)sent.x, (double)sent.y, (double)sent.z, (double)sent.w, (double)landed[0], (double)landed[1],
           (double)landed[2], (double)landed[3]);
    return false;
}

bool TextLanded(const char* field, const char* landed, const std::string& sent) {
    if (landed != nullptr && sent == landed) {
        return true;
    }
    printf("[TEST] FAIL: Notification %s crossed wrong - sent \"%s\", landed \"%s\" (#427)\n", field, sent.c_str(),
           landed != nullptr ? landed : "(null)");
    return false;
}

bool EmitAndVerify(const Notification::Options& sent) {
    Notification::MM_Notify_Emit(sent);

    ComboNotification landed = {};
    if (OoT_Notification_PeekLastForTest(&landed) == 0) {
        printf("[TEST] FAIL: MM's toast never reached OoT's overlay store - the bridge is not wired (#427)\n");
        return false;
    }

    bool ok = true;
    // Pointer identity, not string equality: itemIcon is stored and
    // dereferenced at draw time, so the bridge must forward MM's pointer
    // untouched rather than copy the bytes into storage that dies here.
    if (landed.itemIcon != sent.itemIcon) {
        printf("[TEST] FAIL: Notification itemIcon pointer did not cross intact (#427)\n");
        ok = false;
    }
    ok = TextLanded("prefix", landed.prefix, sent.prefix) && ok;
    ok = ColorLanded("prefixColor", landed.prefixColor, sent.prefixColor) && ok;
    ok = TextLanded("message", landed.message, sent.message) && ok;
    ok = ColorLanded("messageColor", landed.messageColor, sent.messageColor) && ok;
    ok = TextLanded("suffix", landed.suffix, sent.suffix) && ok;
    ok = ColorLanded("suffixColor", landed.suffixColor, sent.suffixColor) && ok;
    if (landed.remainingTime != sent.remainingTime) {
        printf("[TEST] FAIL: Notification remainingTime crossed wrong - sent %f, landed %f (#427)\n",
               (double)sent.remainingTime, (double)landed.remainingTime);
        ok = false;
    }
    if ((landed.mute != 0) != sent.mute) {
        printf("[TEST] FAIL: Notification mute crossed wrong - sent %d, landed %d (#427)\n", (int)sent.mute,
               landed.mute);
        ok = false;
    }
    return ok;
}

} // namespace

extern "C" int MM_NotificationBinding_RunHeadless(void) {
    // Layout first, deliberately. A divergence here means the folded implicit
    // ctor/dtor is already writing past one port's objects, so the payload
    // round trip below can crash (measured: a one-sided field append fail-fasts
    // on MSVC) instead of reporting. Checking layouts before any Options leaves
    // this function is the difference between a named field and a bare SIGSEGV.
    if (!LayoutsAgree()) {
        return 1;
    }

    // Static storage: the overlay keeps itemIcon as a pointer and dereferences
    // it at draw time, which is the contract MM's real caller
    // (Rando::StaticData::GetIconTexturePath) satisfies with static texture
    // data. A local buffer here would pass the compare and still be a lie.
    static const char kIcon[] = "rsbs-notification-bridge-test-icon";

    Notification::Options first;
    first.itemIcon = kIcon;
    first.prefix = "RSBS";
    first.prefixColor = ImVec4(0.25f, 0.5f, 0.75f, 1.0f);
    first.message = "bridge round trip";
    first.messageColor = ImVec4(0.125f, 0.25f, 0.375f, 0.5f);
    first.suffix = "issue 427";
    first.suffixColor = ImVec4(1.0f, 0.75f, 0.5f, 0.25f);
    // Non-zero duration + mute keep OoT's Emit off the CVar store and the audio
    // context, so this row stays headless.
    first.remainingTime = 6.0f;
    first.mute = true;

    if (!EmitAndVerify(first)) {
        return 1;
    }

    // A second, distinctly-valued toast: the first compare could in principle
    // pass against a leftover entry, this one cannot. It also covers the
    // no-icon and empty-affix shapes MM's callers actually use.
    Notification::Options second;
    second.itemIcon = nullptr;
    second.message = "second toast";
    second.suffix = "";
    second.remainingTime = 3.5f;
    second.mute = true;

    if (!EmitAndVerify(second)) {
        return 1;
    }

    printf("[TEST] PASS: mm-notification-binding - MM's toasts reach OoT's overlay through the MM_Notify_Emit bridge "
           "with every field intact, and both ports' Notification::Options stay layout-identical\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
