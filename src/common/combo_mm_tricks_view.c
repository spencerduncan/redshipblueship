/**
 * @file combo_mm_tricks_view.c
 * @brief Registry + value accessors for MM's trick descriptors (#578 part 1).
 *
 * See combo_mm_tricks_view.h for the contract. Like its option-table twin, this
 * file knows nothing about MM: it stores a pointer to a table someone else built
 * and forwards reads/writes to the CVar the descriptor names.
 */

#include "combo_mm_tricks_view.h"
#include "combo_mm_options_view.h" // Combo_MMProfileFrozen — one freeze predicate, not two
#include "foreign_items.h"         // gComboCtx (via context.h), for the log's digest

#include <stdio.h>

#include <libultraship/bridge/consolevariablebridge.h>

static const ComboMMTrickDesc* sTrickTable = NULL;
static int sTrickCount = 0;

void Combo_RegisterMMTrickTable(const ComboMMTrickDesc* table, int count) {
    if (table == NULL && count == 0) {
        sTrickTable = NULL;
        sTrickCount = 0;
        return;
    }
    if (table == NULL || count <= 0) {
        fprintf(stderr, "[MMTricks] table registration rejected: empty table (%d entries)\n", count);
        return;
    }
    if (sTrickTable != NULL && (sTrickTable != table || sTrickCount != count)) {
        // Last writer wins, but not silently — same reasoning as the option
        // table: two DIFFERENT tables over one id space means the pane and the
        // save can disagree with no way to tell which is authoritative.
        //
        // Re-registering the SAME table is silent on purpose. MM_RandoTricksUi_Register
        // is idempotent (it publishes one function-local static vector), and both
        // Combo_MMOptionsWindow_Init and any test that wants the model without a
        // window call it. Warning on that path would fire in the normal case and
        // teach everyone to ignore the message that matters.
        fprintf(stderr, "[MMTricks] a DIFFERENT trick table was registered (%d entries replace %d)\n", count,
                sTrickCount);
    }
    sTrickTable = table;
    sTrickCount = count;
}

int Combo_MMTrickCount(void) {
    return sTrickCount;
}

const ComboMMTrickDesc* Combo_MMTrickAt(int index) {
    if (sTrickTable == NULL || index < 0 || index >= sTrickCount) {
        return NULL;
    }
    return &sTrickTable[index];
}

const ComboMMTrickDesc* Combo_MMTrickById(uint16_t id) {
    for (int i = 0; i < sTrickCount; i++) {
        if (sTrickTable[i].id == id) {
            return &sTrickTable[i];
        }
    }
    return NULL;
}

/** True when the row can legally hold "on" in this build. */
static bool TrickIsSettable(const ComboMMTrickDesc* desc) {
    return desc != NULL && desc->bound && !desc->reserved;
}

bool Combo_MMTrickGetValue(const ComboMMTrickDesc* desc) {
    if (!TrickIsSettable(desc)) {
        // The READER agrees with the writer. A reserved or unbound key reads off
        // whatever its CVar holds, so a stale key (set before it was reserved, or
        // by a hand-edited config) cannot make the pane display an armed trick
        // that MM's own predicate ignores.
        return false;
    }
    return CVarGetInteger(desc->cvar, 0) != 0;
}

void Combo_MMTrickSetValue(const ComboMMTrickDesc* desc, bool on) {
    if (desc == NULL) {
        return;
    }
    if (Combo_MMProfileFrozen()) {
        fprintf(stderr,
                "[MMTricks] write to '%s' REJECTED: the MM profile is frozen into the paired world's "
                "creation identity (digest %08X)\n",
                desc->cvar, (unsigned)gComboCtx.mmProfileDigest);
        return;
    }
    if (!TrickIsSettable(desc)) {
        fprintf(stderr, "[MMTricks] write to '%s' REJECTED: %s\n", desc->cvar,
                desc->disabledReason != NULL ? desc->disabledReason : "not settable in this build");
        return;
    }
    CVarSetInteger(desc->cvar, on ? 1 : 0);
}

void Combo_MMTrickClear(const ComboMMTrickDesc* desc) {
    if (desc == NULL) {
        return;
    }
    // Clearing is a write too: it flips the resolved value, which is folded into
    // the frozen identity. Unlike the setter this is allowed for a reserved or
    // unbound row — removing a key nobody can use is always safe, and it is how
    // the pane's "reset" puts a stale key back to "never chosen".
    if (Combo_MMProfileFrozen()) {
        fprintf(stderr,
                "[MMTricks] clear of '%s' REJECTED: the MM profile is frozen into the paired world's "
                "creation identity (digest %08X)\n",
                desc->cvar, (unsigned)gComboCtx.mmProfileDigest);
        return;
    }
    CVarClear(desc->cvar);
}
