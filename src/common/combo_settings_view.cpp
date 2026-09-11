/**
 * @file combo_settings_view.cpp
 * @brief The tier-4 combo-level settings authoring surface (ADR 0011
 *        increment 2). See combo_settings_view.h for the contract.
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

#include <ship/Context.h>
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
// how a key is spelled.
const ComboSettingDesc kComboSettingDescs[COMBO_SETTING_COUNT] = {
    { RSBS_CVAR_COMBO_RANDO_DIRECTION, "Direction" },
    { RSBS_CVAR_COMBO_RANDO_POOL_SIZE_OOT, "Ocarina of Time items into Majora's Mask checks (max)" },
    { RSBS_CVAR_COMBO_RANDO_POOL_SIZE_MM, "Majora's Mask items into Ocarina of Time checks (max)" },
    { RSBS_CVAR_COMBO_RANDO_ITEM_CLASS_OOT, "Ocarina of Time item classes that may cross" },
    { RSBS_CVAR_COMBO_RANDO_ITEM_CLASS_MM, "Majora's Mask item classes that may cross" },
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

} // extern "C"
