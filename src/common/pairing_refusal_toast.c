/**
 * @file pairing_refusal_toast.c
 * @brief The cross-game refusal toasts' copy (see pairing_refusal_toast.h).
 */
#include "pairing_refusal_toast.h"

#include <stdio.h>
#include <string.h>

/* The arrival's and the MM spoiler's refusals are in the refusal class and take
 * its one prefix (RSBS_REFUSAL_TOAST_PREFIX, #836). The OoT spoiler refusal is
 * not: it refuses a dropped document and latches nothing, and keeps the prefix
 * SoH-side code already used for it. */
#define PREFIX_SPOILER_NOT_LOADED "Spoiler not loaded:"

#define RULES_HEAD "Rules changed"

/* The refused file's words (RsbsRefusalWords), capitalized and with no trailing
 * period, as every message after RSBS_REFUSAL_TOAST_PREFIX is. */
static const char* const kRefusalWords[RSBS_REFUSAL_WORDS_COUNT] = {
    [RSBS_REFUSAL_WORDS_UNREADABLE] = "File could not be read",
    [RSBS_REFUSAL_WORDS_NOT_A_SAVE] = "Not a save file",
    [RSBS_REFUSAL_WORDS_OTHER_BUILD] = "File made by another build",
    [RSBS_REFUSAL_WORDS_WRONG_SLOT] = "File belongs to another slot",
    [RSBS_REFUSAL_WORDS_INCOMPLETE] = "File is incomplete",
    [RSBS_REFUSAL_WORDS_DAMAGED] = "File is damaged",
    [RSBS_REFUSAL_WORDS_RECORD_DAMAGED] = "Cross-game record is damaged",
    /* R-N8: a toast spells the game's name out. */
    [RSBS_REFUSAL_WORDS_OLDER_THAN_OOT] = "Older than the Ocarina of Time save",
    [RSBS_REFUSAL_WORDS_RULES_DIFFER] = "Cross-game rules differ",
    [RSBS_REFUSAL_WORDS_ITEMS_DAMAGED] = "Cross-game items are damaged",
    [RSBS_REFUSAL_WORDS_RECORD_MISSING] = "Cross-game record is missing",
    [RSBS_REFUSAL_WORDS_MM_OPTIONS_DIFFER] = "Majora's Mask options differ",
    [RSBS_REFUSAL_WORDS_NO_MM_WORLD] = "This file has no Majora's Mask world",
    [RSBS_REFUSAL_WORDS_SETTINGS_DIFFER] = "Settings differ from its creation",
    [RSBS_REFUSAL_WORDS_NOT_GENERATED] = "Termina could not be generated",
    [RSBS_REFUSAL_WORDS_UNCHECKED] = "File could not be checked",
};

const char* Combo_RefusalWords(int which) {
    if (which < 0 || which >= RSBS_REFUSAL_WORDS_COUNT || kRefusalWords[which] == NULL) {
        return "";
    }
    return kRefusalWords[which];
}

const char* Combo_PairingRefusalToastPrefix(int kind) {
    switch (kind) {
        case RSBS_PAIRING_REFUSAL_MM_OPTIONS:
        case RSBS_PAIRING_REFUSAL_RULES:
        case RSBS_PAIRING_REFUSAL_MISSING_HALF:
        case RSBS_PAIRING_REFUSAL_SPOILER:
            return RSBS_REFUSAL_TOAST_PREFIX;
        case RSBS_PAIRING_REFUSAL_OOT_SPOILER:
            return PREFIX_SPOILER_NOT_LOADED;
        default:
            return "";
    }
}

static void CopyOut(char* out, size_t len, const char* text) {
    if (out == NULL || len == 0) {
        return;
    }
    snprintf(out, len, "%s", text);
}

/* One field name of Combo_ComboSettingsDivergenceDescribe's list: [begin, end). */
typedef struct FieldSpan {
    const char* begin;
    size_t length;
} FieldSpan;

#define MAX_FIELDS 32

static int SplitFields(const char* detail, FieldSpan* fields) {
    int count = 0;
    const char* p = detail;
    while (p != NULL && *p != '\0' && count < MAX_FIELDS) {
        while (*p == ' ' || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        const char* start = p;
        while (*p != '\0' && *p != ',') {
            p++;
        }
        size_t length = (size_t)(p - start);
        while (length > 0 && start[length - 1] == ' ') {
            length--;
        }
        fields[count].begin = start;
        fields[count].length = length;
        count++;
    }
    return count;
}

/* "Rules changed (<first k names>[ +<rest>])" into buf; returns its length. */
static size_t ComposeRules(const FieldSpan* fields, int count, int named, char* buf, size_t len) {
    size_t used = (size_t)snprintf(buf, len, RULES_HEAD " (");
    for (int i = 0; i < named && used < len; i++) {
        used += (size_t)snprintf(buf + used, len - used, "%s%.*s", i > 0 ? ", " : "", (int)fields[i].length,
                                 fields[i].begin);
    }
    if (named < count && used < len) {
        used += (size_t)snprintf(buf + used, len - used, " +%d", count - named);
    }
    if (used < len) {
        used += (size_t)snprintf(buf + used, len - used, ")");
    }
    return used;
}

static int RulesMessage(const char* detail, char* out, size_t len) {
    FieldSpan fields[MAX_FIELDS];
    const int count = SplitFields(detail, fields);
    if (count == 0) {
        CopyOut(out, len, RULES_HEAD);
        return 0;
    }
    // Name as many fields as fit, in the order the describer lists them (bit
    // order), and count the rest: the leading field is named whenever it fits on
    // its own, so a one-field refusal always says which rule it was.
    char candidate[256];
    for (int named = count; named >= 1; named--) {
        const size_t length = ComposeRules(fields, count, named, candidate, sizeof(candidate));
        if (length <= (size_t)RSBS_PAIRING_REFUSAL_RULES_MAX_CHARS && length < sizeof(candidate)) {
            CopyOut(out, len, candidate);
            return named;
        }
    }
    snprintf(candidate, sizeof(candidate), RULES_HEAD " (%d rules)", count);
    CopyOut(out, len, candidate);
    return 0;
}

/* The MM spoiler loader's refusal routes (Rando/Spoiler/Apply.cpp,
 * ForeignIdentityDiverges), each said in words. The stderr line keeps the
 * machine term and both values. Keyed on the route, never the term: a term the
 * spoiler OMITS is not a term it names differently. */
typedef struct SpoilerRouteCopy {
    const char* route;
    const char* message;
} SpoilerRouteCopy;

static const SpoilerRouteCopy kSpoilerRoutes[] = {
    { RSBS_SPOILER_REFUSAL_NOT_PAIRED, "This spoiler needs a paired file" },
    { RSBS_SPOILER_REFUSAL_SESSION_UNSETTLED, "Spoiler does not match this world" },
    { RSBS_SPOILER_REFUSAL_NO_IDENTITY, "Spoiler has no cross-game identity" },
    { RSBS_SPOILER_REFUSAL_IDENTITY_INCOMPLETE, "Spoiler has an incomplete identity" },
    { RSBS_SPOILER_REFUSAL_OTHER_SEED, "Spoiler is for another seed" },
    { RSBS_SPOILER_REFUSAL_OTHER_SETTINGS, "Spoiler is for other settings" },
    { RSBS_SPOILER_REFUSAL_OTHER_MM_OPTIONS, "Spoiler has other Majora's Mask options" },
};

static int SpoilerMessage(const char* detail, char* out, size_t len) {
    if (detail != NULL) {
        for (size_t i = 0; i < sizeof(kSpoilerRoutes) / sizeof(kSpoilerRoutes[0]); i++) {
            if (strcmp(detail, kSpoilerRoutes[i].route) == 0) {
                CopyOut(out, len, kSpoilerRoutes[i].message);
                return 1;
            }
        }
    }
    CopyOut(out, len, "Spoiler does not match this world");
    return 0;
}

int Combo_PairingRefusalToastMessage(int kind, const char* detail, char* out, size_t len) {
    if (out != NULL && len > 0) {
        out[0] = '\0';
    }
    switch (kind) {
        case RSBS_PAIRING_REFUSAL_MM_OPTIONS:
            CopyOut(out, len, "Majora's Mask options changed");
            return 0;
        case RSBS_PAIRING_REFUSAL_RULES:
            return RulesMessage(detail, out, len);
        case RSBS_PAIRING_REFUSAL_MISSING_HALF:
            // The fact, not a remedy: this leg is reached by a file created before
            // the MM half moved to creation AND by a missing, refused or torn
            // .redsave (GameExports_SingleExe.cpp), and "re-create the file"
            // would throw away progress a backup could restore. The routes and
            // their remedies stay on the stderr line and in the playtest guide.
            // The same words the file select refuses an empty half with.
            return 0;
        case RSBS_PAIRING_REFUSAL_SPOILER:
            return SpoilerMessage(detail, out, len);
        case RSBS_PAIRING_REFUSAL_OOT_SPOILER:
            // "Generate the seed again" is what the stderr line and SPDLOG say;
            // the toast keeps the reason.
            CopyOut(out, len, "it belongs to a paired world.");
            return 0;
        default:
            return 0;
    }
}
