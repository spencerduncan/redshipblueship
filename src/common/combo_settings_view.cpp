/**
 * @file combo_settings_view.cpp
 * @brief The tier-4 combo-level settings authoring surface (ADR 0011
 *        increment 2, plus the goal and #886's Shared Kokiri Sword; #879
 *        retired #668's shared ocarina).
 *        See combo_settings_view.h for the contract.
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
#include "triforce_hunt.h"         // #1389: the Triforce Hunt rows' defaults and bound, the record's sums

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
    { RSBS_CVAR_COMBO_RANDO_GOAL, "Goal" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_KOKIRI_SWORD, "Shared Kokiri Sword" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SHIELDS, "Shared Shields" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_LENS, "Shared Lens of Truth" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SONG_EPONA, "Shared Epona's Song" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SONG_STORMS, "Shared Song of Storms" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_OCARINA_BUTTONS, "Shared Ocarina Buttons" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SONG_TIME, "Shared Song of Time" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_FIRE, "Shared Fire Arrows" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_ICE, "Shared Ice Arrows" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_ARROW_LIGHT, "Shared Light Arrows" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_BOTTLES, "Shared Bottles" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_GORON, "Shared Goron Mask" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_ZORA, "Shared Zora Mask" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_BUNNY, "Shared Bunny Hood" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_KEATON, "Shared Keaton Mask" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_TRUTH, "Shared Mask of Truth" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_SWIM, "Shared Swim" },
    { RSBS_CVAR_COMBO_RANDO_LOGIC, "Logic" }, // #1367: OoTMM's `logic`, under its own name
    // #979: OoTMM's blastMaskOot, sharedMaskBlast and blastMaskCooldown, under
    // OoTMM's own names.
    { RSBS_CVAR_COMBO_RANDO_BLAST_MASK_OOT, "Blast Mask (OoT)" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_BLAST, "Shared Blast Mask" },
    { RSBS_CVAR_COMBO_RANDO_BLAST_MASK_COOLDOWN, "Blast Mask Cooldown" },
    // #1389: OoTMM's `triforcePieces` and `triforceGoal`, under their own names.
    { RSBS_CVAR_COMBO_RANDO_TRIFORCE_PIECES, "Triforce Pieces" },
    { RSBS_CVAR_COMBO_RANDO_TRIFORCE_GOAL, "Triforce Goal" },
    // #979: OoTMM's stoneMaskOot and sharedMaskStone, under OoTMM's own names.
    { RSBS_CVAR_COMBO_RANDO_STONE_MASK_OOT, "Stone Mask (OoT)" },
    { RSBS_CVAR_COMBO_RANDO_SHARED_MASK_STONE, "Shared Stone Mask" },
};

bool ComboSettingIdValid(ComboSettingId id) {
    return (int)id >= 0 && (int)id < (int)COMBO_SETTING_COUNT;
}

// THE ITEM-RULE KEYS (#1068, #1065, #1067, #1069, #970): the settings that author one RSBS_XITEM_* bit of
// the frozen ComboItemRulesRecord rather than a field of ComboSettingsRecord.
// One row per Cross-Game Rules row, in page order (the restore names them in
// this order). Every bit here is a plain 0/1 rule.
struct ItemRuleKey {
    ComboSettingId id;
    uint32_t bit;
};
const ItemRuleKey kItemRuleKeys[] = {
    { COMBO_SETTING_SHARED_SHIELDS, RSBS_XITEM_SHARED_SHIELDS },                 // #1068
    { COMBO_SETTING_SHARED_LENS, RSBS_XITEM_SHARED_LENS },                       // #1065
    { COMBO_SETTING_SHARED_SONG_EPONA, RSBS_XITEM_SHARED_SONG_EPONA },           // #1066
    { COMBO_SETTING_SHARED_SONG_STORMS, RSBS_XITEM_SHARED_SONG_STORMS },         // #1066
    { COMBO_SETTING_SHARED_OCARINA_BUTTONS, RSBS_XITEM_SHARED_OCARINA_BUTTONS }, // #1067
    { COMBO_SETTING_SHARED_SONG_TIME, RSBS_XITEM_SHARED_SONG_TIME },             // #1069
    { COMBO_SETTING_SHARED_ARROW_FIRE, RSBS_XITEM_SHARED_ARROW_FIRE },           // #968
    { COMBO_SETTING_SHARED_ARROW_ICE, RSBS_XITEM_SHARED_ARROW_ICE },             // #968
    { COMBO_SETTING_SHARED_ARROW_LIGHT, RSBS_XITEM_SHARED_ARROW_LIGHT },         // #968
    { COMBO_SETTING_SHARED_BOTTLES, RSBS_XITEM_SHARED_BOTTLES },                 // #970
    { COMBO_SETTING_SHARED_MASK_GORON, RSBS_XITEM_SHARED_GORON },                // #978
    { COMBO_SETTING_SHARED_MASK_ZORA, RSBS_XITEM_SHARED_ZORA },                  // #978
    { COMBO_SETTING_SHARED_MASK_BUNNY, RSBS_XITEM_SHARED_BUNNY },                // #978
    { COMBO_SETTING_SHARED_MASK_KEATON, RSBS_XITEM_SHARED_KEATON },              // #978
    { COMBO_SETTING_SHARED_MASK_TRUTH, RSBS_XITEM_SHARED_TRUTH },                // #978
    { COMBO_SETTING_SHARED_SWIM, RSBS_XITEM_SHARED_SWIM },                       // #1369, shuffle-gated
    { COMBO_SETTING_BLAST_MASK_OOT, RSBS_XITEM_BLAST_MASK_OOT },                 // #979, an extension
    { COMBO_SETTING_STONE_MASK_OOT, RSBS_XITEM_STONE_MASK_OOT },                 // #979, an extension
    { COMBO_SETTING_SHARED_MASK_BLAST, RSBS_XITEM_SHARED_BLAST },                // #979, needs the extension
    { COMBO_SETTING_SHARED_MASK_STONE, RSBS_XITEM_SHARED_STONE },                // #979, needs the extension
};

/** The item-rule bit @p id authors, or 0 for a setting of ComboSettingsRecord. */
uint32_t ItemRuleBitFor(ComboSettingId id) {
    for (const ItemRuleKey& k : kItemRuleKeys) {
        if (k.id == id) {
            return k.bit;
        }
    }
    return 0u;
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
        case COMBO_SETTING_GOAL:
            return (int32_t)defaults.goal;
        case COMBO_SETTING_LOGIC:
            // #1367 R5: All Locations, OoTMM's default (Beatable Only before it).
            return (int32_t)defaults.logicRung;
        case COMBO_SETTING_SHARED_KOKIRI_SWORD:
            // A BIT of the defaults record, read the way its siblings read their
            // fields: one definition of "what ships" (#886).
            return (defaults.comboFlags & (uint8_t)RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD) != 0u ? 1 : 0;
        case COMBO_SETTING_SHARED_SHIELDS:
        case COMBO_SETTING_SHARED_LENS:
        case COMBO_SETTING_SHARED_SONG_EPONA:      // #1066
        case COMBO_SETTING_SHARED_SONG_STORMS:     // #1066
        case COMBO_SETTING_SHARED_OCARINA_BUTTONS: // #1067
        case COMBO_SETTING_SHARED_SONG_TIME:       // #1069
        case COMBO_SETTING_SHARED_ARROW_FIRE:      // #968
        case COMBO_SETTING_SHARED_ARROW_ICE:       // #968
        case COMBO_SETTING_SHARED_ARROW_LIGHT:     // #968
        case COMBO_SETTING_SHARED_BOTTLES:         // #970
        case COMBO_SETTING_SHARED_MASK_GORON:      // #978
        case COMBO_SETTING_SHARED_MASK_ZORA:       // #978
        case COMBO_SETTING_SHARED_MASK_BUNNY:      // #978
        case COMBO_SETTING_SHARED_MASK_KEATON:     // #978
        case COMBO_SETTING_SHARED_MASK_TRUTH:      // #978
        case COMBO_SETTING_BLAST_MASK_OOT:         // #979
        case COMBO_SETTING_SHARED_MASK_BLAST:      // #979
        case COMBO_SETTING_STONE_MASK_OOT:         // #979, the Stone Mask
        case COMBO_SETTING_SHARED_MASK_STONE: {    // #979, the Stone Mask
            // A bit of the item rules' defaults (#1068, #1065, #1066, #1067, #1069, #968, #970, #978, #979),
            // the same one-definition rule: every rule ships off (Combo_ComboItemRulesDefaults).
            ComboItemRulesRecord rulesDefaults;
            Combo_ComboItemRulesDefaults(&rulesDefaults);
            return (rulesDefaults.crossItemFlags & ItemRuleBitFor(id)) != 0u ? 1 : 0;
        }
        case COMBO_SETTING_BLAST_MASK_COOLDOWN: {
            // #979: the item rules' cooldown BYTE, read off the same defaults
            // record (RSBS_BLAST_COOLDOWN_DEFAULT: Majora's Mask's own cooldown).
            ComboItemRulesRecord rulesDefaults;
            Combo_ComboItemRulesDefaults(&rulesDefaults);
            return (int32_t)rulesDefaults.blastMaskCooldown;
        }
        case COMBO_SETTING_SHARED_SWIM:
            // #1369 (operator ruling 2026-11-02): the one item rule that ships
            // ON. Still the one definition of "what ships": its bit is
            // shuffle-gated, so the defaults RECORD stays all clear (a world
            // nobody shuffles swim in never freezes it), and only a world that
            // shuffles swim somewhere freezes the bit this key authors.
            return 1;
        case COMBO_SETTING_TRIFORCE_PIECES:
            // #1389: OoTMM's defaults. No record field holds them (the triforce
            // record stores their split, and only under the hunt goal), so
            // triforce_hunt.h is their one definition.
            return (int32_t)RSBS_TRIFORCE_COMBO_DEFAULT_PIECES;
        case COMBO_SETTING_TRIFORCE_GOAL:
            return (int32_t)RSBS_TRIFORCE_COMBO_DEFAULT_GOAL;
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
        case COMBO_SETTING_GOAL:
            // The pinned enumerators and nothing else, the direction's rule. 0
            // is a legacy record's "unset" (ADR 0010 D1's growth contract: it
            // makes no beatability claim) and is never a choice; a value past
            // the table is a goal this build has no evaluator for.
            return value == (int32_t)RSBS_COMBO_GOAL_BEAT_BOTH || value == (int32_t)RSBS_COMBO_GOAL_BEAT_EITHER ||
                   value == (int32_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT || value == (int32_t)RSBS_COMBO_GOAL_BEAT_OOT ||
                   value == (int32_t)RSBS_COMBO_GOAL_BEAT_MM;
        case COMBO_SETTING_LOGIC:
            // #1367: the three pinned rungs and nothing else. 0 is a legacy
            // record's "unset"; a value past the table is a rung this build has
            // no fill for.
            return value == (int32_t)RSBS_COMBO_RUNG_NONE || value == (int32_t)RSBS_COMBO_RUNG_BEATABLE ||
                   value == (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE;
        case COMBO_SETTING_SHARED_KOKIRI_SWORD:
        case COMBO_SETTING_SHARED_SHIELDS:         // #1068: one crossItemFlags bit, the same rule
        case COMBO_SETTING_SHARED_LENS:            // #1065: likewise
        case COMBO_SETTING_SHARED_SONG_EPONA:      // #1066, the same
        case COMBO_SETTING_SHARED_SONG_STORMS:     // #1066, the same
        case COMBO_SETTING_SHARED_OCARINA_BUTTONS: // #1067: likewise
        case COMBO_SETTING_SHARED_SONG_TIME:       // #1069: likewise
        case COMBO_SETTING_SHARED_ARROW_FIRE:      // #968, the same
        case COMBO_SETTING_SHARED_ARROW_ICE:       // #968, the same
        case COMBO_SETTING_SHARED_ARROW_LIGHT:     // #968, the same
        case COMBO_SETTING_SHARED_BOTTLES:         // #970: likewise
        case COMBO_SETTING_SHARED_MASK_GORON:      // #978, the same
        case COMBO_SETTING_SHARED_MASK_ZORA:       // #978, the same
        case COMBO_SETTING_SHARED_MASK_BUNNY:      // #978, the same
        case COMBO_SETTING_SHARED_MASK_KEATON:     // #978, the same
        case COMBO_SETTING_SHARED_MASK_TRUTH:      // #978, the same
        case COMBO_SETTING_SHARED_SWIM:            // #1369, the same
        case COMBO_SETTING_BLAST_MASK_OOT:         // #979, the same
        case COMBO_SETTING_SHARED_MASK_BLAST:      // #979, the same
        case COMBO_SETTING_STONE_MASK_OOT:         // #979, the same
        case COMBO_SETTING_SHARED_MASK_STONE:      // #979, the same
            // EXACTLY 0 or 1, not "nonzero is true" (#886, #668's rule). A flag key
            // holding 2 is a value nobody chose, and the record stores one bit,
            // so a coercion would make two stored values produce the same world.
            return value == 0 || value == 1;
        case COMBO_SETTING_BLAST_MASK_COOLDOWN:
            // #979: the six pinned RSBS_BLAST_COOLDOWN_* values and nothing else; a
            // value past the table is a cooldown this build has no frame count for.
            return value >= (int32_t)RSBS_BLAST_COOLDOWN_DEFAULT && value <= (int32_t)RSBS_BLAST_COOLDOWN_MAX;
        case COMBO_SETTING_TRIFORCE_PIECES: // #1389
        case COMBO_SETTING_TRIFORCE_GOAL:
            // 1..255: a hunt places at least one piece and asks for at least one,
            // and OoT's 8-bit counter mirrors the one count (OoTMM's 999 would
            // wrap it). The goal's own bound, the pieces, is a relation between
            // two keys: the resolver clamps it (Combo_ResolveComboTriforce).
            return value >= 1 && value <= (int32_t)RSBS_TRIFORCE_COMBO_MAX;
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

uint8_t Combo_ComboSettingsLogicRungShown(void) {
    // The frozen record's own rung while a world is decided, as the Cross-Game
    // rows show the frozen record; else what the next creation resolves. A
    // frozen rung outside the pinned space (a later build's) is never shown as
    // a rung this build would run.
    if (Combo_ComboSettingsFrozen()) {
        const uint8_t frozen = gComboCtx.comboSettings.logicRung;
        return Combo_ComboSettingValueValid(COMBO_SETTING_LOGIC, (int32_t)frozen)
                   ? frozen
                   : (uint8_t)Combo_ComboSettingDefault(COMBO_SETTING_LOGIC);
    }
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    return live.logicRung;
}

bool Combo_ComboSettingsTriforceHuntShown(void) {
    // The rung's rule (Combo_ComboSettingsLogicRungShown): the frozen record's
    // goal while a world is decided, else the goal the next creation resolves.
    if (Combo_ComboSettingsFrozen()) {
        return gComboCtx.comboSettings.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
    }
    ComboSettingsRecord live;
    Combo_ResolveComboSettings(&live);
    return live.goal == (uint8_t)RSBS_COMBO_GOAL_TRIFORCE_HUNT;
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

namespace {

// The rules a key authors, in the page's order (Goal first, then the
// crossing), so the notification reads the way the page does. Every other
// divergence bit is either a field no key writes or damage to the stored
// identity.
struct Restorable {
    uint32_t bit;
    ComboSettingId id;
};
const Restorable kRestorable[] = {
    { RSBS_COMBO_DIVERGE_GOAL, COMBO_SETTING_GOAL },
    // #1367: the logic rung has a key now, so a file made at one rung and met by
    // a session staged at another puts its own rung back, like the goal. Every
    // world made before the key froze Beatable Only, the key's default then; the
    // default is All Locations since #1367 R5, and those worlds load as they were.
    { RSBS_COMBO_DIVERGE_LOGIC_RUNG, COMBO_SETTING_LOGIC },
    // #1389: the Triforce Hunt's pieces and goal, beside the goal they serve.
    // One divergence bit for both keys; each is written only when it differs.
    // Their values are the file's triforce record's sums (FrozenValue).
    { RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS, COMBO_SETTING_TRIFORCE_PIECES },
    { RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS, COMBO_SETTING_TRIFORCE_GOAL },
    { RSBS_COMBO_DIVERGE_DIRECTION, COMBO_SETTING_DIRECTION },
    { RSBS_COMBO_DIVERGE_SHARED_KOKIRI_SWORD, COMBO_SETTING_SHARED_KOKIRI_SWORD },
    // The item rules (#976) diverge as ONE bit, crossItemFlags; each keyed rule
    // (kItemRuleKeys) is one row under it (#1068, #1065, #1066, #1067, #1069, #968,
    // #970, #978).
    // Restorable only when every differing bit has a key (ItemRulesKeyedDiff).
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_SHIELDS },
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_LENS },
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_SONG_EPONA },      // #1066
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_SONG_STORMS },     // #1066
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_OCARINA_BUTTONS }, // #1067
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_SONG_TIME },       // #1069
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_ARROW_FIRE },      // #968
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_ARROW_ICE },       // #968
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_ARROW_LIGHT },     // #968
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_BOTTLES },         // #970
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_GORON },      // #978
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_ZORA },       // #978
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_BUNNY },      // #978
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_KEATON },     // #978
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_TRUTH },      // #978
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_SWIM },            // #1369
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_BLAST_MASK_OOT },         // #979
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_STONE_MASK_OOT },         // #979
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_BLAST },      // #979
    { RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS, COMBO_SETTING_SHARED_MASK_STONE },      // #979
    // #979: the cooldown BYTE has its own divergence bit and, since #979, a key.
    { RSBS_COMBO_DIVERGE_BLAST_MASK_COOLDOWN, COMBO_SETTING_BLAST_MASK_COOLDOWN },
};

/** Can the keys put the file's crossItemFlags back? Only when every bit that
 *  differs from the session's resolution has a key; a rule only another build
 *  authors (an extension mask, say) cannot be restored, and the load refuses it by
 *  name. */
bool ItemRulesKeyedDiff(const ComboItemRulesRecord* frozenRules) {
    ComboItemRulesRecord live;
    Combo_ResolveComboItemRules(&live);
    Combo_ComboItemRulesLiveForFile(frozenRules, &live); // #1369: a gated rule as the file decided it
    const uint32_t frozenFlags = (frozenRules != nullptr) ? frozenRules->crossItemFlags : 0u;
    return ((frozenFlags ^ live.crossItemFlags) & ~Combo_ComboItemRuleKeyedBits()) == 0u;
}

int32_t FrozenValue(const ComboSettingsRecord* frozen, const ComboItemRulesRecord* frozenRules,
                    const ComboTriforceRecord* frozenTriforce, ComboSettingId id) {
    switch (id) {
        case COMBO_SETTING_TRIFORCE_PIECES:
            // #1389: the record's sums (Combo_TriforceRecordTotal is 0 for NULL,
            // which is outside the key's space, so no record restores nothing).
            return (int32_t)Combo_TriforceRecordTotal(frozenTriforce);
        case COMBO_SETTING_TRIFORCE_GOAL:
            return (int32_t)Combo_TriforceRecordRequired(frozenTriforce);
        case COMBO_SETTING_DIRECTION:
            return (int32_t)frozen->direction;
        case COMBO_SETTING_GOAL:
            return (int32_t)frozen->goal;
        case COMBO_SETTING_LOGIC:
            return (int32_t)frozen->logicRung; // #1367
        case COMBO_SETTING_SHARED_KOKIRI_SWORD:
            return (frozen->comboFlags & (uint8_t)RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD) != 0u ? 1 : 0;
        case COMBO_SETTING_SHARED_SHIELDS:
        case COMBO_SETTING_SHARED_LENS:
        case COMBO_SETTING_SHARED_SONG_EPONA:  // #1066
        case COMBO_SETTING_SHARED_SONG_STORMS: // #1066
        case COMBO_SETTING_SHARED_OCARINA_BUTTONS:
        case COMBO_SETTING_SHARED_SONG_TIME:
        case COMBO_SETTING_SHARED_ARROW_FIRE:
        case COMBO_SETTING_SHARED_ARROW_ICE:
        case COMBO_SETTING_SHARED_ARROW_LIGHT:
        case COMBO_SETTING_SHARED_BOTTLES:
        case COMBO_SETTING_SHARED_MASK_GORON:
        case COMBO_SETTING_SHARED_MASK_ZORA:
        case COMBO_SETTING_SHARED_MASK_BUNNY:
        case COMBO_SETTING_SHARED_MASK_KEATON:
        case COMBO_SETTING_SHARED_MASK_TRUTH:
        case COMBO_SETTING_SHARED_SWIM:       // #1369
        case COMBO_SETTING_BLAST_MASK_OOT:    // #979
        case COMBO_SETTING_SHARED_MASK_BLAST: // #979
        case COMBO_SETTING_STONE_MASK_OOT:    // #979
        case COMBO_SETTING_SHARED_MASK_STONE: // #979
            // A bit of the file's item rules (NULL: every rule off).
            return (frozenRules != nullptr && (frozenRules->crossItemFlags & ItemRuleBitFor(id)) != 0u) ? 1 : 0;
        case COMBO_SETTING_BLAST_MASK_COOLDOWN:
            // #979: the file's cooldown byte (NULL: the default).
            return frozenRules != nullptr ? (int32_t)frozenRules->blastMaskCooldown
                                          : (int32_t)RSBS_BLAST_COOLDOWN_DEFAULT;
        default:
            return 0;
    }
}

/** Write @p value into @p id's key for the restore, recording the undo and the
 *  label. */
void RestoreKey(ComboSettingId id, int32_t value, ComboSettingsKeyUndo* undo, std::string& restored) {
    const char* key = kComboSettingDescs[id].key;
    if (undo != nullptr) {
        undo->written[id] = 1;
        undo->wasSet[id] = Combo_CVarIsExplicitInt(key) ? 1 : 0;
        undo->value[id] = CVarGetInteger(key, 0);
    }
    CVarSetInteger(key, value);
    if (!restored.empty()) {
        restored += ", ";
    }
    restored += kComboSettingDescs[id].label;
    std::fprintf(stderr, "[Combo] load: '%s' restored to the file's value %d (frozen wins at load, #781)\n", key,
                 (int)value);
}

} // namespace

uint32_t Combo_ComboItemRulesAuthored(void) {
    // Each key through THE ONE READER, so an unset key and an out-of-space
    // stored value are both the shipped default (off), with the reason logged.
    uint32_t bits = 0u;
    for (const ItemRuleKey& k : kItemRuleKeys) {
        if (Combo_ComboSettingResolved(k.id) != 0) {
            bits |= k.bit;
        }
    }
    return bits;
}

uint32_t Combo_ComboItemRuleKeyedBits(void) {
    uint32_t bits = 0u;
    for (const ItemRuleKey& k : kItemRuleKeys) {
        bits |= k.bit;
    }
    return bits;
}

uint32_t Combo_ComboSettingsRestorableMask(void) {
    uint32_t mask = 0;
    for (const Restorable& r : kRestorable) {
        mask |= r.bit;
    }
    return mask;
}

uint32_t Combo_ComboSettingsUnrestorableBits(const ComboItemRulesRecord* frozenRules, uint32_t divergedBits) {
    uint32_t bits = divergedBits & ~Combo_ComboSettingsRestorableMask();
    if ((divergedBits & RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) != 0u && !ItemRulesKeyedDiff(frozenRules)) {
        bits |= RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS;
    }
    return bits;
}

int Combo_ComboSettingsCanRestoreLive(const ComboSettingsRecord* frozen, const ComboItemRulesRecord* frozenRules,
                                      uint32_t divergedBits) {
    return Combo_ComboSettingsCanRestoreLiveEx(frozen, frozenRules, nullptr, divergedBits);
}

int Combo_ComboSettingsCanRestoreLiveEx(const ComboSettingsRecord* frozen, const ComboItemRulesRecord* frozenRules,
                                        const ComboTriforceRecord* frozenTriforce, uint32_t divergedBits) {
    if (frozen == nullptr || divergedBits == 0) {
        return 0;
    }
    if (Combo_ComboSettingsUnrestorableBits(frozenRules, divergedBits) != 0u) {
        // A field no key authors (an item rule only another build authors among
        // them, #1068), or damage: the caller refuses.
        return 0;
    }
    if (!Combo_ComboSettingStoreAvailable()) {
        return 0;
    }
    // All-or-nothing: every value is validated before any is written, so a
    // record carrying an out-of-space value leaves the store exactly as it was.
    // An item rule's value is one bit, 0 or 1, so it is always in its space.
    for (const Restorable& r : kRestorable) {
        if ((divergedBits & r.bit) != 0u &&
            !Combo_ComboSettingValueValid(r.id, FrozenValue(frozen, frozenRules, frozenTriforce, r.id))) {
            return 0;
        }
    }
    return 1;
}

int Combo_ComboSettingsRestoreLive(const ComboSettingsRecord* frozen, const ComboItemRulesRecord* frozenRules,
                                   uint32_t divergedBits, char* names, size_t len, ComboSettingsKeyUndo* undo) {
    return Combo_ComboSettingsRestoreLiveEx(frozen, frozenRules, nullptr, divergedBits, names, len, undo);
}

int Combo_ComboSettingsRestoreLiveEx(const ComboSettingsRecord* frozen, const ComboItemRulesRecord* frozenRules,
                                     const ComboTriforceRecord* frozenTriforce, uint32_t divergedBits, char* names,
                                     size_t len, ComboSettingsKeyUndo* undo) {
    if (names != nullptr && len > 0) {
        names[0] = '\0';
    }
    if (undo != nullptr) {
        *undo = ComboSettingsKeyUndo{};
    }
    // The same answer the file-select probe got (#836), so the probe and the
    // load cannot disagree about whether the file can restore its rules.
    if (Combo_ComboSettingsCanRestoreLiveEx(frozen, frozenRules, frozenTriforce, divergedBits) == 0) {
        return 0;
    }

    std::string restored;
    for (const Restorable& r : kRestorable) {
        if ((divergedBits & r.bit) == 0u) {
            continue;
        }
        // One divergence bit covers every item rule (crossItemFlags), and one
        // both Triforce Hunt numbers (#1389): write only the keys whose value
        // actually differs, so a load names what it changed. Every differing
        // rule is keyed (checked above), so this makes the resolver's
        // crossItemFlags the file's exactly.
        const int32_t value = FrozenValue(frozen, frozenRules, frozenTriforce, r.id);
        if ((r.bit == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS || r.bit == RSBS_COMBO_DIVERGE_TRIFORCE_COUNTS) &&
            Combo_ComboSettingResolved(r.id) == value) {
            continue;
        }
        // #1369: a shuffle-gated rule the file froze OFF is the rule inert at
        // creation (no game shuffled its item), whatever the key said then; it
        // agrees with a key either way (Combo_ComboItemRulesLiveForFile), so the
        // key the player set is left alone.
        if (r.bit == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS && value == 0 &&
            (ItemRuleBitFor(r.id) & (uint32_t)RSBS_XITEM_SHUFFLE_GATED) != 0u) {
            continue;
        }
        RestoreKey(r.id, value, undo, restored);
    }
    if (names != nullptr && len > 0) {
        std::snprintf(names, len, "%s", restored.c_str());
    }
    return 1;
}

void Combo_ComboSettingsRestoreUndo(const ComboSettingsKeyUndo* undo) {
    if (undo == nullptr || !Combo_ComboSettingStoreAvailable()) {
        return;
    }
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (undo->written[i] == 0) {
            continue;
        }
        const char* key = kComboSettingDescs[i].key;
        if (undo->wasSet[i] != 0) {
            CVarSetInteger(key, undo->value[i]);
            std::fprintf(stderr, "[Combo] load: '%s' put back to the session's %d (the restore did not take)\n", key,
                         (int)undo->value[i]);
        } else {
            CVarClear(key);
            std::fprintf(stderr, "[Combo] load: '%s' put back to unset (the restore did not take)\n", key);
        }
    }
}

void Combo_ComboSettingsPersistStore(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr || ctx->GetWindow()->GetGui() == nullptr) {
        return;
    }
    ctx->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

} // extern "C"
