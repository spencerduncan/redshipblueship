/**
 * @file pairing_refusal_toast.c
 * @brief The cross-game refusal toasts' copy (see pairing_refusal_toast.h).
 */
#include "pairing_refusal_toast.h"

#include <stdio.h>
#include <string.h>

/* Every "Not saved:" refusal latches the active unified-save slot for the
 * session (RsbsSave_RefuseSlotIdentity / RsbsSave_RefuseSlotGeneration), so the
 * outcome the player must not miss is that nothing this session is saved to the
 * pair. The OoT spoiler refusal latches nothing: it refuses the document, and
 * keeps the prefix SoH-side code already used for it. */
#define PREFIX_NOT_SAVED "Not saved:"
#define PREFIX_SPOILER_NOT_LOADED "Spoiler not loaded:"

#define RULES_HEAD "rules changed"

const char* Combo_PairingRefusalToastPrefix(int kind) {
    switch (kind) {
        case RSBS_PAIRING_REFUSAL_MM_OPTIONS:
        case RSBS_PAIRING_REFUSAL_RULES:
        case RSBS_PAIRING_REFUSAL_MISSING_HALF:
        case RSBS_PAIRING_REFUSAL_SPOILER:
            return PREFIX_NOT_SAVED;
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

/* "rules changed (<first k names>[ +<rest>])." into buf; returns its length. */
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
        used += (size_t)snprintf(buf + used, len - used, ").");
    }
    return used;
}

static int RulesMessage(const char* detail, char* out, size_t len) {
    FieldSpan fields[MAX_FIELDS];
    const int count = SplitFields(detail, fields);
    if (count == 0) {
        CopyOut(out, len, RULES_HEAD ".");
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
    snprintf(candidate, sizeof(candidate), RULES_HEAD " (%d rules).", count);
    CopyOut(out, len, candidate);
    return 0;
}

/* The MM spoiler loader's identity terms (Rando/Spoiler/Apply.cpp,
 * ForeignIdentityDiverges), each said in words. The stderr line keeps the
 * machine term and both values. */
typedef struct SpoilerTermCopy {
    const char* term;
    const char* message;
} SpoilerTermCopy;

static const SpoilerTermCopy kSpoilerTerms[] = {
    { "sourceIsRando", "this spoiler needs a paired file." },
    { "rsbsPairing", "spoiler has no cross-game identity." },
    { "sharedRandoSeed", "spoiler is for another seed." },
    { "sharedRandoSettingsHash", "spoiler is for other settings." },
    { "mmProfileDigest", "spoiler has other Majora's Mask options." },
};

static int SpoilerMessage(const char* detail, char* out, size_t len) {
    if (detail != NULL) {
        for (size_t i = 0; i < sizeof(kSpoilerTerms) / sizeof(kSpoilerTerms[0]); i++) {
            if (strcmp(detail, kSpoilerTerms[i].term) == 0) {
                CopyOut(out, len, kSpoilerTerms[i].message);
                return 1;
            }
        }
    }
    CopyOut(out, len, "spoiler is for another world.");
    return 0;
}

int Combo_PairingRefusalToastMessage(int kind, const char* detail, char* out, size_t len) {
    if (out != NULL && len > 0) {
        out[0] = '\0';
    }
    switch (kind) {
        case RSBS_PAIRING_REFUSAL_MM_OPTIONS:
            CopyOut(out, len, "Majora's Mask options changed.");
            return 0;
        case RSBS_PAIRING_REFUSAL_RULES:
            return RulesMessage(detail, out, len);
        case RSBS_PAIRING_REFUSAL_MISSING_HALF:
            // The one remedy for every route here (a file created before the MM
            // half moved to creation, or a missing or refused .redsave).
            CopyOut(out, len, "no Majora's Mask world; re-create the file.");
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
