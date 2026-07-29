/**
 * @file notification_bridge.h
 * @brief Explicit cross-game surface for the shared Gui notification overlay
 *        (#427 item 1; ADR 0002).
 *
 * ONE toast overlay renders over whichever game is active — that behavior is
 * deliberate and wanted. How MM used to reach it was not: MM's
 * 2s2h/BenGui/Notification.cpp is link-excluded in single-exe builds
 * (games/mm/CMakeLists.txt drops 2s2h/BenGui/*.cpp), so every
 * `Notification::Emit(Options)` MM compiled bound OoT's
 * soh/Notification/Notification.cpp definition — not through any declared
 * interface, but because `Notification::Emit(Notification::Options)` happens to
 * mangle the same in both ports and the two `struct Options` happen to be
 * field-identical. Add, drop, reorder, or retype a field in EITHER port and
 * OoT's Emit reads a differently laid out argument: silent misrender or worse,
 * with no link error possible (exactly one Emit definition survives, so the
 * linker has nothing to reject — the #382 FlagTable/culling class).
 *
 * This header is that missing interface. No C++ type crosses the boundary at
 * all: MM packs its own Options into the plain-C ComboNotification below and
 * calls OoT_Notification_Emit, which unpacks it into OoT's own Options by NAME
 * and calls OoT's Emit. Consequences:
 *
 *  - No Options is ever passed to a body compiled against the other port's
 *    view, so the argument-passing corruption above is gone.
 *  - Drift that the bridge maps — a renamed, removed, or retyped field — is a
 *    compile error in the drifting port's own TU, because both sides read/write
 *    their Options by field name and both compile the same
 *    COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT list below.
 *  - MM's single-exe view of 2s2h/BenGui/Notification.h no longer declares
 *    `Emit` at all, so a new MM call site that tries to re-arm the coincidence
 *    is a compile error naming the bridge.
 *
 * WHAT THE BRIDGE DOES NOT RETIRE: the two ports still declare the same
 * `Notification::Options` type name (entry `TYPE Options` of
 * .github/odr-declaration-baseline.txt), and the struct is non-trivial, so the
 * implicitly-defined `Options::Options()` / `Options::~Options()` each port's
 * TUs emit are COMDAT duplicates that the linker folds to one. MM constructs
 * and destroys Options in MM TUs, so the two layouts must still be EQUAL or the
 * surviving copy runs against the other port's objects. That half is locked at
 * runtime by src/common/notification_layout_probe.h — see its header comment;
 * retiring it needs the type names to stop colliding, not this bridge.
 *
 * Ownership: the implementation lives on the OoT side
 * (games/oot/soh/Notification/Notification.cpp), the only place the surviving
 * Emit and the overlay's store exist. MM's half is
 * `Notification::MM_Notify_Emit` (games/mm/2s2h/mm_notification_bridge.cpp).
 */

#ifndef RSBS_COMMON_NOTIFICATION_BRIDGE_H
#define RSBS_COMMON_NOTIFICATION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wire form of one toast. Plain C on purpose: this struct is the ONLY thing
 * that crosses the game boundary, so it must be identical in both TUs by
 * construction rather than by coincidence — no std:: types, no ImGui types, no
 * per-port headers.
 *
 * LIFETIME. Strings are copied into the overlay's own std::string storage
 * during the call, so the caller's buffers only need to survive the call.
 * `itemIcon` is the exception: it is stored as a pointer and dereferenced at
 * draw time, frames later, so it must point at storage that outlives the toast
 * (MM's only caller passes Rando::StaticData::GetIconTexturePath, i.e. static
 * texture data). This is the same contract the direct bind already had.
 */
typedef struct ComboNotification {
    const char* itemIcon;  // texture name, or NULL for no icon; must outlive the toast
    const char* prefix;    // NULL is read as ""
    float prefixColor[4];  // RGBA
    const char* message;   // NULL is read as ""
    float messageColor[4]; // RGBA
    const char* suffix;    // NULL is read as ""
    float suffixColor[4];  // RGBA
    float remainingTime;   // Seconds; 0 means "use the overlay's configured duration"
    int mute;              // Non-zero suppresses the notification sound
} ComboNotification;

// Queue one toast on the shared overlay. Implemented on the OoT side
// (soh/Notification/Notification.cpp); MM reaches it through
// Notification::MM_Notify_Emit. A NULL argument is a no-op.
//
// The toast's id is assigned by the overlay, which is why ComboNotification
// carries no id field.
void OoT_Notification_Emit(const ComboNotification* notification);

// Test-only readback of the most recently queued toast, for the
// mm-notification-binding lock. Returns 0 (leaving *out untouched) when the
// overlay's store is empty, 1 otherwise. The returned string pointers alias the
// stored toast, so they are valid only until the store is next mutated.
int OoT_Notification_PeekLastForTest(ComboNotification* out);

#ifdef __cplusplus
} // extern "C"

#include <cstdint>
#include <string>
#include <type_traits>

/**
 * The field/type half of the contract, compiled by BOTH ports against their own
 * `Notification::Options` (soh/Notification/Notification.cpp and
 * games/mm/2s2h/mm_notification_bridge.cpp; the lock TU compiles it too). One
 * list, stated once, so the two views cannot drift apart silently the way the
 * duplicated per-port assert sets they replace could.
 *
 * Naming a field here also makes its rename or removal a compile error in the
 * port that dropped it. A field APPENDED to one port's Options is not visible
 * here — it simply never reaches the wire — which is why the layout-equality
 * half of the lock (src/common/notification_layout_probe.h) is still required:
 * an append is what folds a wrong-sized implicit constructor/destructor onto
 * the other port's objects.
 *
 * Expands to a run of namespace-scope static_asserts; invoke with a trailing
 * semicolon. Requires ImVec4 in scope (both ports have it via their
 * Notification.h).
 */
#define COMBO_NOTIFICATION_ASSERT_OPTIONS_CONTRACT(OptionsType)                                                        \
    static_assert(std::is_same_v<decltype(OptionsType::id), uint32_t>,                                                 \
                  "Notification::Options::id drifted from the bridge contract (#427)");                                \
    static_assert(std::is_same_v<decltype(OptionsType::itemIcon), const char*>,                                        \
                  "Notification::Options::itemIcon drifted from the bridge contract (#427)");                          \
    static_assert(std::is_same_v<decltype(OptionsType::prefix), std::string>,                                          \
                  "Notification::Options::prefix drifted from the bridge contract (#427)");                            \
    static_assert(std::is_same_v<decltype(OptionsType::prefixColor), ImVec4>,                                          \
                  "Notification::Options::prefixColor drifted from the bridge contract (#427)");                       \
    static_assert(std::is_same_v<decltype(OptionsType::message), std::string>,                                         \
                  "Notification::Options::message drifted from the bridge contract (#427)");                           \
    static_assert(std::is_same_v<decltype(OptionsType::messageColor), ImVec4>,                                         \
                  "Notification::Options::messageColor drifted from the bridge contract (#427)");                      \
    static_assert(std::is_same_v<decltype(OptionsType::suffix), std::string>,                                          \
                  "Notification::Options::suffix drifted from the bridge contract (#427)");                            \
    static_assert(std::is_same_v<decltype(OptionsType::suffixColor), ImVec4>,                                          \
                  "Notification::Options::suffixColor drifted from the bridge contract (#427)");                       \
    static_assert(std::is_same_v<decltype(OptionsType::remainingTime), float>,                                         \
                  "Notification::Options::remainingTime drifted from the bridge contract (#427)");                     \
    static_assert(std::is_same_v<decltype(OptionsType::mute), bool>,                                                   \
                  "Notification::Options::mute drifted from the bridge contract (#427)")

#endif // __cplusplus

#endif // RSBS_COMMON_NOTIFICATION_BRIDGE_H
