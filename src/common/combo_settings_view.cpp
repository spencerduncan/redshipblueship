/**
 * @file combo_settings_view.cpp
 * @brief The tier-4 combo-level settings authoring surface (ADR 0011
 *        increment 2, plus the shared ocarina #668). See
 *        combo_settings_view.h for the contract.
 *
 * C++ rather than C for exactly one reason: the CVar store hangs off the
 * Ship::Context singleton and libultraship's C bridge dereferences that
 * singleton unconditionally, so a read has to be guarded on its existence
 * before the bridge is touched. Nothing else here needs C++; the whole
 * surface is C-linkage so foreign_items.c (C) and the pane (C++) call the same
 * functions.
 */

#include "combo_settings_view.h"

#include <cstdio>

#include <string>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_mm_options_view.h" // Combo_CVarIsExplicitInt: the same unset probe the MM pane uses
#include "cvar_shared_keys.h"      // the RSBS_CVAR_COMBO_RANDO_* key literals
#include "foreign_items.h"         // the pinned value spaces, the defaults, Combo_ComboSettingsFrozen

namespace {

struct ComboSettingDesc {
    const char* key;
    const char* label;
};

// Indexed by ComboSettingId. The keys are the manifest's literals, not local
// copies, so the classification lock and this table cannot disagree about
// how a key is spelled. Appended to, never reordered: the menu's staging
// buffers and the locks' expectation tables are both indexed by the id.
const ComboSettingDesc kComboSettingDescs[COMBO_SETTING_COUNT] = {
    { RSBS_CVAR_COMBO_RANDO_DIRECTION, "Crossing Direction" },
    { RSBS_CVAR_COMBO_RANDO_POOL_SIZE_OOT, "Max OoT Items on MM Checks" },
    { RSBS_CVAR_COMBO_RANDO_POOL_SIZE_MM, "Max MM Items on OoT Checks" },
    { RSBS_CVAR_COMBO_RANDO_ITEM_CLASS_OOT, "OoT Item Classes" },
    { RSBS_CVAR_COMBO_RANDO_ITEM_CLASS_MM, "MM Item Classes" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_OCARINA, "Shared Ocarina" },
    { RSBS_CVAR_COMBO_RANDO_GOAL, "Goal" },
};

bool ComboSettingIdValid(ComboSettingId id) {
    return (int)id >= 0 && (int)id < (int)COMBO_SETTING_COUNT;
}

} // namespace

extern "C" {

const char* Combo_ComboSettingKey(ComboSettingId id) {
    return ComboSettingIdValid(id) ? kComboSettingDescs[id].key : "(invalid)";
}

const char* Combo_ComboSettingLabel(ComboSettingId id) {
    return ComboSettingIdValid(id) ? kComboSettingDescs[id].label : "(invalid)";
}

int32_t Combo_ComboSettingDefault(ComboSettingId id) {
    // ONE definition of "what ships": the defaults record, field by field.
    // Restating the numbers here would be a second default that can drift
    // from the one the O5 transitional writer freezes.
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    switch (id) {
        case COMBO_SETTING_DIRECTION:
            return (int32_t)defaults.direction;
        case COMBO_SETTING_POOL_SIZE_OOT:
            return (int32_t)defaults.poolSizeOoT;
        case COMBO_SETTING_POOL_SIZE_MM:
            return (int32_t)defaults.poolSizeMM;
        case COMBO_SETTING_ITEM_CLASS_OOT:
            return (int32_t)defaults.itemClassOoT;
        case COMBO_SETTING_ITEM_CLASS_MM:
            return (int32_t)defaults.itemClassMM;
        case COMBO_SETTING_SHARED_OCARINA:
            // A BIT of the defaults record, read the same way its siblings read
            // their fields: one definition of "what ships", even when the field
            // is a bitset and the key is one of its bits (#668).
            return (defaults.comboFlags & (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA) != 0u ? 1 : 0;
        case COMBO_SETTING_GOAL:
            return (int32_t)defaults.goal;
        default:
            return 0;
    }
}

bool Combo_ComboSettingValueValid(ComboSettingId id, int32_t value) {
    switch (id) {
        case COMBO_SETTING_DIRECTION:
            // The pinned enumerators and nothing else. 0 is unreachable inside
            // a formatted record (decision 1.3) and is therefore not authorable
            // either; "a new enumerator" is exactly what an out-of-table value
            // must never become.
            return value == (int32_t)RSBS_COMBO_DIR_OFF || value == (int32_t)RSBS_COMBO_DIR_FORWARD ||
                   value == (int32_t)RSBS_COMBO_DIR_REVERSE || value == (int32_t)RSBS_COMBO_DIR_BOTH;
        case COMBO_SETTING_POOL_SIZE_OOT:
        case COMBO_SETTING_POOL_SIZE_MM:
            // Accepted answer O4: 1..CAP. A count that can exceed the table's
            // capacity is a setting that lies; a count of 0 is not a pool size
            // (the direction byte is what says "off").
            return value >= 1 && value <= (int32_t)RSBS_FOREIGN_PLACEMENT_CAP;
        case COMBO_SETTING_ITEM_CLASS_OOT:
        case COMBO_SETTING_ITEM_CLASS_MM:
            // A uint16 mask over the ALLOCATED bits only: an unallocated bit
            // must read 0 in a formatVersion-1 record (decision 1.2.1), so it
            // must not be authorable. Zero is legal (decision 3.3).
            return value >= 0 && value <= 0xFFFF && (((uint32_t)value & ~(uint32_t)RSBS_ITEMCLASS_ALL_V1) == 0u);
        case COMBO_SETTING_SHARED_OCARINA:
            // EXACTLY 0 or 1, not "nonzero is true" (#668). A flag key holding 2
            // is a value nobody chose, and this file's rule for those is the
            // shipped default with a logged reason -- not a coercion that
            // silently turns a typo into "on". The record stores it as one bit,
            // so a coercion here would also make two different stored values
            // produce the same world, which is precisely what the pinned value
            // spaces exist to prevent.
            return value == 0 || value == 1;
        case COMBO_SETTING_GOAL:
            // The pinned enumerators and nothing else, the direction's rule. 0
            // is a legacy record's "unset" (ADR 0010 D1's growth contract: it
            // makes no beatability claim) and is never a choice; a value past
            // the table is a goal this build has no evaluator for.
            return value == (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH || value == (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER ||
                   value == (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT || value == (int32_t)RSBS_COMBO_GOAL_BEAT_OOT ||
                   value == (int32_t)RSBS_COMBO_GOAL_BEAT_MM;
        default:
            return false;
    }
}

bool Combo_ComboSettingStoreAvailable(void) {
    // Both halves, because CreateUninitializedInstance leaves ConsoleVariables
    // null until the shared bring-up runs: a context with no store is the
    // harness's pre-boot state, and the bridge would dereference it just the
    // same.
    auto ctx = Ship::Context::GetInstance();
    return ctx != nullptr && ctx->GetConsoleVariables() != nullptr;
}

bool Combo_ComboSettingReadStore(ComboSettingId id, int32_t* out) {
    if (!ComboSettingIdValid(id) || out == nullptr || !Combo_ComboSettingStoreAvailable()) {
        return false;
    }
    const char* key = kComboSettingDescs[id].key;
    if (!Combo_CVarIsExplicitInt(key)) {
        return false; // unset, or set with a type no integer read can see
    }
    *out = CVarGetInteger(key, Combo_ComboSettingDefault(id));
    return true;
}

int32_t Combo_ComboSettingResolved(ComboSettingId id) {
    if (!ComboSettingIdValid(id)) {
        return 0;
    }
    const int32_t fallback = Combo_ComboSettingDefault(id);
    int32_t raw = fallback;
    if (!Combo_ComboSettingReadStore(id, &raw)) {
        return fallback; // nothing authored: the shipped default, silently
    }
    if (Combo_ComboSettingValueValid(id, raw)) {
        return raw;
    }

    // Out of its pinned space. This can only have arrived out-of-band (the
    // writers refuse it), and the rule is: the SHIPPED DEFAULT, with the reason
    // in the log — never a new enumerator, never a clamp that invents a value
    // nobody chose. Logged once per distinct offending value per key, because
    // the resolver runs on every unfrozen read (each placement pass, each
    // arrival compare) and a repeated complaint buries the one that matters.
    static int32_t sComplainedValue[COMBO_SETTING_COUNT];
    static bool sComplained[COMBO_SETTING_COUNT];
    if (!sComplained[id] || sComplainedValue[id] != raw) {
        std::fprintf(stderr,
                     "[Combo] setting '%s' = %d is outside its pinned value space; resolving to the shipped "
                     "default %d (an out-of-space value is never promoted to a new enumerator)\n",
                     kComboSettingDescs[id].key, (int)raw, (int)fallback);
        sComplained[id] = true;
        sComplainedValue[id] = raw;
    }
    return fallback;
}

int Combo_ComboSettingSet(ComboSettingId id, int32_t value) {
    if (!ComboSettingIdValid(id)) {
        return 0;
    }
    const char* key = kComboSettingDescs[id].key;

    // THE GATE (ADR 0004 §6 state 4; #564 V5): post-creation the rules are
    // world identity, not a setting. Rejecting here — rather than only greying
    // the pane — is the actual enforcement: the pane is one caller, and every
    // future caller of the writer inherits it. The reason is "already
    // decided", not a capability reason: nothing is unavailable, it was chosen.
    if (Combo_ComboSettingsFrozen()) {
        std::fprintf(stderr,
                     "[Combo] write to '%s' REJECTED: the combo rules are frozen into the paired world's creation "
                     "identity (fingerprint %08X) — already decided\n",
                     key, (unsigned)gComboCtx.comboSettingsHash);
        return 0;
    }
    if (!Combo_ComboSettingValueValid(id, value)) {
        std::fprintf(stderr, "[Combo] write to '%s' REJECTED: %d is outside its pinned value space\n", key, (int)value);
        return 0;
    }
    if (!Combo_ComboSettingStoreAvailable()) {
        std::fprintf(stderr, "[Combo] write to '%s' REJECTED: no CVar store in this process\n", key);
        return 0;
    }
    CVarSetInteger(key, value);
    return 1;
}

int Combo_ComboSettingClear(ComboSettingId id) {
    if (!ComboSettingIdValid(id)) {
        return 0;
    }
    const char* key = kComboSettingDescs[id].key;
    // Clearing is a write too: it changes the resolved value, which is folded
    // into the identity the next arrival compares against.
    if (Combo_ComboSettingsFrozen()) {
        std::fprintf(stderr,
                     "[Combo] clear of '%s' REJECTED: the combo rules are frozen into the paired world's creation "
                     "identity (fingerprint %08X) — already decided\n",
                     key, (unsigned)gComboCtx.comboSettingsHash);
        return 0;
    }
    if (!Combo_ComboSettingStoreAvailable()) {
        std::fprintf(stderr, "[Combo] clear of '%s' REJECTED: no CVar store in this process\n", key);
        return 0;
    }
    CVarClear(key);
    return 1;
}

bool Combo_ComboSettingIsExplicit(ComboSettingId id) {
    if (!ComboSettingIdValid(id) || !Combo_ComboSettingStoreAvailable()) {
        return false;
    }
    return Combo_CVarIsExplicitInt(kComboSettingDescs[id].key);
}

const char* Combo_ComboSettingReadOnlyReason(void) {
    // The SAME predicate the writers gate on, so a pane drawn from this string
    // and a write refused by Combo_ComboSettingSet can never disagree. NULL
    // while editable, because "no reason" and "an empty reason" are different
    // facts and only one of them is renderable.
    return Combo_ComboSettingsFrozen() ? "already decided" : NULL;
}

const char* Combo_ComboSettingSharedMarker(void) {
    // ADR 0004 §4.2's affordance, as a badge the row NAME carries rather than a
    // tooltip: "applies to both games" must be legible without hovering, and a
    // player who toggles a crossing rule under Ocarina of Time and later finds
    // Majora's Mask changed would otherwise read correct behaviour as a bug.
    // Text rather than an icon so the same string is searchable in the menu's
    // own search, assertable from a headless row lock, and legible in the one
    // place a font-atlas glyph would not be.
    return "[Both Games]";
}

const char* Combo_ComboDirectionName(uint8_t direction) {
    switch (direction) {
        case (uint8_t)RSBS_COMBO_DIR_OFF:
            return "off";
        case (uint8_t)RSBS_COMBO_DIR_FORWARD:
            return "forward";
        case (uint8_t)RSBS_COMBO_DIR_REVERSE:
            return "reverse";
        case (uint8_t)RSBS_COMBO_DIR_BOTH:
            return "both";
        default:
            return "(unknown)";
    }
}

int Combo_ComboSettingsRestoreLive(const ComboSettingsRecord* frozen, uint32_t divergedBits, char* names, size_t len) {
    if (names != nullptr && len > 0) {
        names[0] = '\0';
    }
    if (frozen == nullptr || divergedBits == 0) {
        return 0;
    }

    // The rules a key authors, in the page's order (Goal first, then the
    // crossing), so the notification reads the way the page does. Every other
    // bit is either a field no key writes or damage to the stored identity.
    struct Restorable {
        uint32_t bit;
        ComboSettingId id;
        int32_t value;
    };
    const Restorable kRestorable[] = {
        { RSBS_COMBO_DIVERGE_GOAL, COMBO_SETTING_GOAL, (int32_t)frozen->goal },
        { RSBS_COMBO_DIVERGE_DIRECTION, COMBO_SETTING_DIRECTION, (int32_t)frozen->direction },
        { RSBS_COMBO_DIVERGE_POOL_SIZE_OOT, COMBO_SETTING_POOL_SIZE_OOT, (int32_t)frozen->poolSizeOoT },
        { RSBS_COMBO_DIVERGE_POOL_SIZE_MM, COMBO_SETTING_POOL_SIZE_MM, (int32_t)frozen->poolSizeMM },
        { RSBS_COMBO_DIVERGE_ITEM_CLASS_OOT, COMBO_SETTING_ITEM_CLASS_OOT, (int32_t)frozen->itemClassOoT },
        { RSBS_COMBO_DIVERGE_ITEM_CLASS_MM, COMBO_SETTING_ITEM_CLASS_MM, (int32_t)frozen->itemClassMM },
        { RSBS_COMBO_DIVERGE_SHARED_OCARINA, COMBO_SETTING_SHARED_OCARINA,
          (frozen->comboFlags & (uint8_t)RSBS_COMBO_FLAG_SHARED_OCARINA) != 0u ? 1 : 0 },
    };

    uint32_t restorableMask = 0;
    for (const Restorable& r : kRestorable) {
        restorableMask |= r.bit;
    }
    if ((divergedBits & ~restorableMask) != 0u) {
        return 0; // a field no key authors, or damage: the caller refuses
    }
    if (!Combo_ComboSettingStoreAvailable()) {
        return 0;
    }
    // All-or-nothing: validate every value before writing any, so a record
    // carrying an out-of-space value leaves the store exactly as it was.
    for (const Restorable& r : kRestorable) {
        if ((divergedBits & r.bit) != 0u && !Combo_ComboSettingValueValid(r.id, r.value)) {
            return 0;
        }
    }

    std::string restored;
    for (const Restorable& r : kRestorable) {
        if ((divergedBits & r.bit) == 0u) {
            continue;
        }
        CVarSetInteger(kComboSettingDescs[r.id].key, r.value);
        if (!restored.empty()) {
            restored += ", ";
        }
        restored += kComboSettingDescs[r.id].label;
        std::fprintf(stderr, "[Combo] load: '%s' restored to the file's value %d (frozen wins at load, #781)\n",
                     kComboSettingDescs[r.id].key, (int)r.value);
    }
    if (names != nullptr && len > 0) {
        std::snprintf(names, len, "%s", restored.c_str());
    }
    return 1;
}

void Combo_ComboSettingsPersistStore(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr || ctx->GetWindow()->GetGui() == nullptr) {
        return;
    }
    ctx->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

} // extern "C"
