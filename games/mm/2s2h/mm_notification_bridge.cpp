/**
 * MM's half of the cross-game notification bridge (#427 item 1).
 *
 * The shared Gui overlay is owned by OoT (soh/Notification/Notification.cpp);
 * MM's own 2s2h/BenGui/Notification.cpp is link-excluded in single-exe builds.
 * MM toasts used to reach that overlay by ABI coincidence — MM's
 * `Notification::Emit(Options)` call mangled the same as OoT's definition, so
 * it bound OoT's body and passed it MM's struct. Correct only while the two
 * ports' Options stayed field-identical, and unlockable at link time: exactly
 * one Emit definition survives, so there is no duplicate for the linker to
 * reject (the #382 FlagTable/culling class).
 *
 * This TU is the declared replacement. It is the ONLY place in MM that touches
 * the cross-game notification path, and what it hands across is plain-C
 * ComboNotification data (src/common/notification_bridge.h) — no C++ type
 * crosses the boundary, so no Options ever reaches a body compiled against the
 * other port's view. MM's single-exe view of 2s2h/BenGui/Notification.h no
 * longer declares `Emit`, so a future MM call site cannot silently re-arm the
 * coincidence; it fails to compile and names this bridge.
 *
 * The two layouts must still be EQUAL for a second reason the bridge does not
 * touch: both trees declare the same `Notification::Options` type name, so the
 * implicit constructor/destructor this TU emits COMDAT-fold with OoT's. That
 * half is locked at runtime by src/common/notification_layout_probe.h.
 *
 * Drift that still matters is a compile error here: every field is read by
 * name, and the shared COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT list — the
 * same one OoT's side compiles against its Options — pins each field's type.
 *
 * The end-to-end lock is games/mm/2s2h/mm_notification_binding_test.cpp
 * (CTest row mm-notification-binding).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/BenGui/Notification.h"
#include "notification_bridge.h"

#include <type_traits>

COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT(Notification::Options);

namespace Notification {

void MM_Notify_Emit(const Options& notification) {
    ComboNotification wire = {};

    // c_str() is safe across the call: the overlay copies the text into its own
    // storage before returning. itemIcon is NOT copied — it is dereferenced at
    // draw time — so callers must pass storage that outlives the toast, which
    // is why it is forwarded as the pointer MM was given rather than reburied.
    wire.itemIcon = notification.itemIcon;
    wire.prefix = notification.prefix.c_str();
    wire.prefixColor[0] = notification.prefixColor.x;
    wire.prefixColor[1] = notification.prefixColor.y;
    wire.prefixColor[2] = notification.prefixColor.z;
    wire.prefixColor[3] = notification.prefixColor.w;
    wire.message = notification.message.c_str();
    wire.messageColor[0] = notification.messageColor.x;
    wire.messageColor[1] = notification.messageColor.y;
    wire.messageColor[2] = notification.messageColor.z;
    wire.messageColor[3] = notification.messageColor.w;
    wire.suffix = notification.suffix.c_str();
    wire.suffixColor[0] = notification.suffixColor.x;
    wire.suffixColor[1] = notification.suffixColor.y;
    wire.suffixColor[2] = notification.suffixColor.z;
    wire.suffixColor[3] = notification.suffixColor.w;
    wire.remainingTime = notification.remainingTime;
    wire.mute = notification.mute ? 1 : 0;

    // Options::id is not carried: the overlay assigns it on arrival.
    OoT_Notification_Emit(&wire);
}

} // namespace Notification

#endif // RSBS_SINGLE_EXECUTABLE
