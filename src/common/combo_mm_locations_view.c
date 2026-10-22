/**
 * @file combo_mm_locations_view.c
 * @brief Registry, reader and gated writers for Randomizer > MM Locations
 *        (#1101 lane R3).
 *
 * See combo_mm_locations_view.h for the contract. Like the option and trick
 * views, this file knows nothing about MM: it stores a pointer to a table
 * someone else built, parses and writes the one CVar MM's pool generation
 * reads, and refuses the writes the freeze and the logic make meaningless.
 */

#include "combo_mm_locations_view.h"
#include "combo_mm_options_page.h" // COMBO_RANDO_PAGES_NEXT_WORLD_NOTE, Combo_RandoPages_FrozenNote
#include "combo_mm_options_view.h" // Combo_MMProfileFrozen: one freeze predicate, not three
#include "foreign_items.h"         // gComboCtx (via context.h), for the log's digest and the fingerprint

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libultraship/bridge/consolevariablebridge.h>

static const ComboMMLocationDesc* sLocationTable = NULL;
static int sLocationCount = 0;
static bool (*sVanillaLogic)(void) = NULL;

void Combo_RegisterMMLocationTable(const ComboMMLocationDesc* table, int count, bool (*vanillaLogic)(void)) {
    if (table == NULL && count == 0) {
        sLocationTable = NULL;
        sLocationCount = 0;
        sVanillaLogic = NULL;
        return;
    }
    if (table == NULL || count <= 0) {
        fprintf(stderr, "[MMLocations] table registration rejected: empty table (%d entries)\n", count);
        return;
    }
    if (sLocationTable != NULL && (sLocationTable != table || sLocationCount != count)) {
        // Re-registering the SAME table is silent (the registrar is idempotent
        // and runs from every page bring-up); a DIFFERENT one is named, as the
        // option and trick registries do.
        fprintf(stderr, "[MMLocations] a DIFFERENT check table was registered (%d entries replace %d)\n", count,
                sLocationCount);
    }
    sLocationTable = table;
    sLocationCount = count;
    sVanillaLogic = vanillaLogic;
}

int Combo_MMLocationCount(void) {
    return sLocationCount;
}

const ComboMMLocationDesc* Combo_MMLocationAt(int index) {
    if (sLocationTable == NULL || index < 0 || index >= sLocationCount) {
        return NULL;
    }
    return &sLocationTable[index];
}

const ComboMMLocationDesc* Combo_MMLocationById(uint16_t id) {
    for (int i = 0; i < sLocationCount; i++) {
        if (sLocationTable[i].id == id) {
            return &sLocationTable[i];
        }
    }
    return NULL;
}

// ---- the list ------------------------------------------------------------------

/** One bit per id over the whole uint16_t id space, so a hand-written id the
 *  table does not hold survives a rewrite instead of being dropped unseen. */
#define LOCATION_ID_SPACE 65536
typedef struct {
    uint8_t bits[LOCATION_ID_SPACE / 8];
} LocationSet;

static void SetPut(LocationSet* set, uint16_t id, bool on) {
    if (on) {
        set->bits[id >> 3] |= (uint8_t)(1u << (id & 7));
    } else {
        set->bits[id >> 3] &= (uint8_t) ~(1u << (id & 7));
    }
}

static bool SetHas(const LocationSet* set, uint16_t id) {
    return ((set->bits[id >> 3] >> (id & 7)) & 1u) != 0;
}

/** Parse the key the way MM's pool generation reads it (comma-separated
 *  decimal ids); a token that is not one is skipped. */
static void ReadKey(LocationSet* set) {
    memset(set, 0, sizeof(*set));
    const char* raw = CVarGetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, "");
    if (raw == NULL) {
        return;
    }
    const char* p = raw;
    while (*p != '\0') {
        char* end = NULL;
        const long id = strtol(p, &end, 10);
        if (end != p && id >= 0 && id < LOCATION_ID_SPACE) {
            SetPut(set, (uint16_t)id, true);
        }
        // Skip to the next token whatever this one held.
        while (*p != '\0' && *p != ',') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }
}

/** Write @p set in the one spelling (ascending ids, each followed by a comma),
 *  or clear the key when it is empty. */
static void WriteKey(const LocationSet* set) {
    size_t cap = 64;
    size_t len = 0;
    char* out = (char*)malloc(cap);
    if (out == NULL) {
        return;
    }
    out[0] = '\0';
    for (int id = 0; id < LOCATION_ID_SPACE; id++) {
        if (!SetHas(set, (uint16_t)id)) {
            continue;
        }
        char token[8];
        const int n = snprintf(token, sizeof(token), "%d,", id);
        if (len + (size_t)n + 1 > cap) {
            while (len + (size_t)n + 1 > cap) {
                cap *= 2;
            }
            char* grown = (char*)realloc(out, cap);
            if (grown == NULL) {
                free(out);
                return;
            }
            out = grown;
        }
        memcpy(out + len, token, (size_t)n + 1);
        len += (size_t)n;
    }
    if (len == 0) {
        CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    } else {
        CVarSetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, out);
    }
    free(out);
}

int Combo_MMLocationsExcluded(uint16_t* out, int cap) {
    static LocationSet set;
    ReadKey(&set);
    int count = 0;
    for (int id = 0; id < LOCATION_ID_SPACE; id++) {
        if (SetHas(&set, (uint16_t)id)) {
            if (out != NULL && count < cap) {
                out[count] = (uint16_t)id;
            }
            count++;
        }
    }
    return count;
}

bool Combo_MMLocationIsExcluded(uint16_t id) {
    static LocationSet set;
    ReadKey(&set);
    return SetHas(&set, id);
}

bool Combo_MMLocationsBlockedByLogic(void) {
    return sVanillaLogic != NULL && sVanillaLogic();
}

bool Combo_MMLocationsSetExcluded(const uint16_t* ids, int count, bool excluded) {
    if (Combo_MMProfileFrozen()) {
        fprintf(stderr,
                "[MMLocations] write to '%s' REJECTED: the MM profile is frozen into the paired world's creation "
                "identity (digest %08X)\n",
                COMBO_MM_EXCLUDED_CHECKS_CVAR, (unsigned)gComboCtx.mmProfileDigest);
        return false;
    }
    if (Combo_MMLocationsBlockedByLogic()) {
        // ADR 0004 section 5: under Vanilla logic MM's pool generation never
        // reads the list, but the digest folds it, so an edit would move the
        // identity of a world that plays identically.
        fprintf(stderr, "[MMLocations] write to '%s' REJECTED: %s\n", COMBO_MM_EXCLUDED_CHECKS_CVAR,
                COMBO_MM_LOCATIONS_VANILLA_REASON);
        return false;
    }
    if (ids == NULL || count <= 0) {
        return true;
    }
    static LocationSet set;
    ReadKey(&set);
    bool changed = false;
    for (int i = 0; i < count; i++) {
        if (Combo_MMLocationById(ids[i]) == NULL || SetHas(&set, ids[i]) == excluded) {
            continue; // not a check the page lists, or already where it is asked to be
        }
        SetPut(&set, ids[i], excluded);
        changed = true;
    }
    if (changed) {
        WriteKey(&set);
    }
    return true;
}

bool Combo_MMLocationSetExcluded(uint16_t id, bool excluded) {
    return Combo_MMLocationsSetExcluded(&id, 1, excluded);
}

bool Combo_MMLocationsClearExcluded(void) {
    if (Combo_MMProfileFrozen()) {
        fprintf(stderr,
                "[MMLocations] clear of '%s' REJECTED: the MM profile is frozen into the paired world's creation "
                "identity (digest %08X)\n",
                COMBO_MM_EXCLUDED_CHECKS_CVAR, (unsigned)gComboCtx.mmProfileDigest);
        return false;
    }
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    return true;
}

const char* Combo_MMLocationsPage_StatusNote(void) {
    if (Combo_MMProfileFrozen()) {
        // The sentence every randomizer page of ours shares (#1101 decision 7).
        static char frozen[192];
        return Combo_RandoPages_FrozenNote(gComboCtx.comboSettingsHash, frozen, sizeof(frozen));
    }
    if (Combo_MMLocationsBlockedByLogic()) {
        return "Vanilla logic leaves every item where it always is, so Majora's Mask's locations cannot be changed.";
    }
    return COMBO_RANDO_PAGES_NEXT_WORLD_NOTE;
}
