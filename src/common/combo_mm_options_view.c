/**
 * @file combo_mm_options_view.c
 * @brief Registry + value accessors for MM's randomizer option descriptors.
 *
 * See combo_mm_options_view.h for the contract. This file deliberately knows
 * nothing about MM: it stores a pointer to a table someone else built and
 * forwards value reads/writes to the CVar the descriptor names.
 */

#include "combo_mm_options_view.h"
#include "foreign_items.h" // gComboCtx (via context.h) + Combo_ForeignPairingActive

#include <stdio.h>
#include <string.h>

#include <libultraship/bridge/consolevariablebridge.h>

static const ComboMMOptionDesc* sOptionTable = NULL;
static int sOptionCount = 0;

void Combo_RegisterMMOptionTable(const ComboMMOptionDesc* table, int count) {
    if (table == NULL && count == 0) {
        // Explicit un-register (see the header): a test that installs a
        // synthetic table must be able to put the registry back.
        sOptionTable = NULL;
        sOptionCount = 0;
        return;
    }
    if (table == NULL || count <= 0) {
        fprintf(stderr, "[MMOptions] table registration rejected: empty table (%d entries)\n", count);
        return;
    }
    if (sOptionTable != NULL) {
        // Last writer wins, but not silently: two tables describing one option
        // id space means the pane's labels and the save's values can disagree
        // with no way to tell which is authoritative.
        fprintf(stderr, "[MMOptions] option table re-registered (%d entries replace %d)\n", count, sOptionCount);
    }
    sOptionTable = table;
    sOptionCount = count;
}

int Combo_MMOptionCount(void) {
    return sOptionCount;
}

const ComboMMOptionDesc* Combo_MMOptionAt(int index) {
    if (sOptionTable == NULL || index < 0 || index >= sOptionCount) {
        return NULL;
    }
    return &sOptionTable[index];
}

const ComboMMOptionDesc* Combo_MMOptionById(uint16_t id) {
    for (int i = 0; i < sOptionCount; i++) {
        if (sOptionTable[i].id == id) {
            return &sOptionTable[i];
        }
    }
    return NULL;
}

const char* Combo_MMOptionGroupName(uint8_t group) {
    switch ((ComboMMOptionGroup)group) {
        case COMBO_MM_GROUP_LOGIC:
            return "Logic & Conditions";
        case COMBO_MM_GROUP_SHUFFLE:
            return "Shuffle Options";
        case COMBO_MM_GROUP_ITEMS:
            return "Items";
        case COMBO_MM_GROUP_STARTING:
            return "Starting Items";
        case COMBO_MM_GROUP_HINTS:
            return "Hints";
        default:
            // Visible placeholder rather than NULL: every caller here is a
            // printf-family or ImGui text call, and a NULL would be an invalid
            // pointer rather than an empty section header.
            return "(unknown group)";
    }
}

/** The descriptor's legal value range, as [lo, hi]. */
static void OptionRange(const ComboMMOptionDesc* desc, int32_t* lo, int32_t* hi) {
    switch ((ComboMMOptionWidget)desc->widget) {
        case COMBO_MM_WIDGET_CHECKBOX:
            *lo = 0;
            *hi = 1;
            return;
        case COMBO_MM_WIDGET_COMBO:
            *lo = 0;
            // A zero-length combo would give hi == -1 and clamp every write to
            // -1. The table lock forbids that shape; this keeps a broken table
            // from writing a negative index into RANDO_SAVE_OPTIONS anyway.
            *hi = (desc->valueCount > 0) ? (int32_t)desc->valueCount - 1 : 0;
            return;
        case COMBO_MM_WIDGET_SLIDER:
        case COMBO_MM_WIDGET_TIME:
        default:
            *lo = desc->minValue;
            *hi = desc->maxValue;
            return;
    }
}

int32_t Combo_MMOptionGetValue(const ComboMMOptionDesc* desc) {
    if (desc == NULL) {
        return 0;
    }
    return CVarGetInteger(desc->cvar, desc->defaultValue);
}

void Combo_MMOptionSetValue(const ComboMMOptionDesc* desc, int32_t value) {
    if (desc == NULL) {
        return;
    }
    // #498/#564: post-creation the profile is world identity, not a setting.
    // Rejecting here (rather than only greying the pane) is the actual gate —
    // the pane is one caller, but any future caller of the writer inherits it.
    if (Combo_MMProfileFrozen()) {
        fprintf(stderr,
                "[MMOptions] write to '%s' REJECTED: the MM profile is frozen into the paired world's "
                "creation identity (digest %08X)\n",
                desc->cvar, (unsigned)gComboCtx.mmProfileDigest);
        return;
    }
    int32_t lo = 0;
    int32_t hi = 0;
    OptionRange(desc, &lo, &hi);
    if (value < lo) {
        value = lo;
    }
    if (value > hi) {
        value = hi;
    }
    CVarSetInteger(desc->cvar, value);
}

bool Combo_CVarIsExplicitInt(const char* cvar) {
    if (cvar == NULL) {
        return false;
    }
    // See the header for why this is a probe rather than CVarExists. The two
    // sentinels must differ from each other and be values no option can hold;
    // the integer extremes satisfy both by construction.
    const int32_t lo = CVarGetInteger(cvar, INT32_MIN);
    const int32_t hi = CVarGetInteger(cvar, INT32_MAX);
    return lo == hi;
}

bool Combo_MMOptionIsExplicit(const ComboMMOptionDesc* desc) {
    if (desc == NULL) {
        return false;
    }
    return Combo_CVarIsExplicitInt(desc->cvar);
}

void Combo_MMOptionClear(const ComboMMOptionDesc* desc) {
    if (desc == NULL) {
        return;
    }
    // Clearing is a write too (see the header): it changes the resolved value
    // and the logic pin's explicitness, both folded into the frozen identity.
    if (Combo_MMProfileFrozen()) {
        fprintf(stderr,
                "[MMOptions] clear of '%s' REJECTED: the MM profile is frozen into the paired world's "
                "creation identity (digest %08X)\n",
                desc->cvar, (unsigned)gComboCtx.mmProfileDigest);
        return;
    }
    CVarClear(desc->cvar);
}

bool Combo_MMProfileFrozen(void) {
    // The creation-stamped digest IS the predicate (#564 V5): a src/common
    // fact, never a gSaveContext read — ADR 0008 rule 5 and this pane's own
    // game-agnosticism tripwire both forbid the latter.
    return gComboCtx.mmProfileDigest != 0;
}

void Combo_MMProfileSummary(ComboMMProfileSummary* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    // Same predicate the spoiler view uses: "a paired world exists" is a
    // post-condition of a successful generation, not an intent (ADR 0009
    // decision 2). An unpaired world must not present gComboCtx's zeros as a
    // real seed.
    out->paired = Combo_ForeignPairingActive();
    if (!out->paired) {
        return;
    }
    out->sharedRandoSeed = gComboCtx.sharedRandoSeed;
    out->sharedRandoSettingsHash = gComboCtx.sharedRandoSettingsHash;
    out->mmProfileDigest = gComboCtx.mmProfileDigest;
}
