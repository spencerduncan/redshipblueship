/**
 * @file combo_hint_view.c
 * @brief The combined Hint Tracker's view model, Majora's Mask pane (#1475,
 *        lane H1). See combo_hint_view.h.
 *
 * Everything here is a read but for one write, the read record's bit, which
 * MM's read hook asks for (Combo_HintMarkRead). The reads: gComboCtx's record,
 * the MM save image the check tracker's MM pane reads (Combo_TrackerMMSave),
 * and the MM adapter's resolver over it. No game header, no ImGui, no caching:
 * the pane is recomputed per call, like the check tracker's.
 */

#include "combo_hint_view.h"

#include "context.h" // gComboCtx.mmHintsRead, RSBS_MM_HINT_READ_CAP
#include "game.h"    // GAME_MM

#include <stdio.h>
#include <string.h>

RSBS_CTX_STATIC_ASSERT(RSBS_MM_HINT_SRC_COUNT <= RSBS_MM_HINT_READ_CAP,
                       "an MM hint source has no bit in ComboContext.mmHintsRead: raise RSBS_MM_HINT_READ_CAP with a "
                       "carve (ADR 0009), never truncate");
RSBS_CTX_STATIC_ASSERT(RSBS_MM_HINT_READ_CAP % 8u == 0u &&
                           sizeof(((ComboContext*)0)->mmHintsRead) * 8u == RSBS_MM_HINT_READ_CAP,
                       "the read record is whole bytes, one bit per source id");

// ============================================================================
// The resolution helpers
// ============================================================================

void Combo_HintResolutionClear(ComboHintResolution* r) {
    if (r != NULL) {
        memset(r, 0, sizeof(*r));
    }
}

bool Combo_HintResolutionAddTarget(ComboHintResolution* r, const ComboHintTarget* t) {
    if (r == NULL || t == NULL || r->targetCount >= RSBS_HINT_TARGET_CAP) {
        return false;
    }
    r->targets[r->targetCount++] = *t;
    return true;
}

bool Combo_HintResolutionAddArea(ComboHintResolution* r, ComboArea a) {
    if (r == NULL || r->areaCount >= RSBS_HINT_TARGET_CAP ||
        (a.game != (uint8_t)GAME_OOT && a.game != (uint8_t)GAME_MM)) {
        return false;
    }
    r->areas[r->areaCount++] = a;
    return true;
}

bool Combo_HintResolutionSetText(ComboHintResolution* r, const char* text) {
    if (r == NULL) {
        return false;
    }
    const int n = snprintf(r->text, sizeof(r->text), "%s", text != NULL ? text : "");
    return n >= 0 && (size_t)n < sizeof(r->text);
}

int Combo_HintPlainText(const char* msg, char* out, int cap) {
    if (out != NULL && cap > 0) {
        out[0] = '\0';
    }
    if (msg == NULL) {
        return 0;
    }
    int len = 0;        // the full length, as if `out` were unbounded
    bool space = false; // a space is pending (collapsed, and never leading)
    for (const char* p = msg; *p != '\0'; p++) {
        const unsigned char c = (unsigned char)*p;
        if (c == '%' && p[1] != '\0' && strchr("rgybpw", p[1]) != NULL) {
            p++; // MM's colour codes (CustomMessage::ReplaceColorChars)
            continue;
        }
        if (c < 0x20 || c >= 0x7F || c == ' ') {
            // A line or box break, a sound or end code, or a space: one space at most.
            space = len > 0;
            continue;
        }
        if (space) {
            if (out != NULL && len + 1 < cap) {
                out[len] = ' ';
            }
            len++;
            space = false;
        }
        if (out != NULL && len + 1 < cap) {
            out[len] = (char)c;
        }
        len++;
    }
    if (out != NULL && cap > 0) {
        out[len < cap ? len : cap - 1] = '\0';
    }
    return len;
}

// ============================================================================
// The MM adapter registry
// ============================================================================

static ComboMMHintDesc sMMHintDesc;
static bool sMMHintRegistered = false;

void Combo_Hint_RegisterMM(const ComboMMHintDesc* desc) {
    if (desc == NULL) {
        sMMHintRegistered = false;
        memset(&sMMHintDesc, 0, sizeof(sMMHintDesc));
        return;
    }
    if (desc->sourceInfo == NULL || desc->listed == NULL || desc->resolve == NULL ||
        desc->sourceCount > RSBS_MM_HINT_READ_CAP) {
        fprintf(stderr,
                "[HintTracker] REJECTED MM hint descriptor: a NULL member or %u sources past the read "
                "record's %u\n",
                (unsigned)desc->sourceCount, (unsigned)RSBS_MM_HINT_READ_CAP);
        return;
    }
    sMMHintDesc = *desc;
    sMMHintRegistered = true;
}

const ComboMMHintDesc* Combo_Hint_GetMMDesc(void) {
    return sMMHintRegistered ? &sMMHintDesc : NULL;
}

// ============================================================================
// The read record
// ============================================================================

bool Combo_HintIsRead(uint8_t game, uint16_t source) {
    if (game != (uint8_t)GAME_MM || source >= RSBS_MM_HINT_READ_CAP) {
        return false;
    }
    return ((gComboCtx.mmHintsRead[source >> 3] >> (source & 7u)) & 1u) != 0;
}

bool Combo_HintMarkRead(uint8_t game, uint16_t source) {
    if (game != (uint8_t)GAME_MM || source >= RSBS_MM_HINT_READ_CAP || Combo_HintIsRead(game, source)) {
        return false;
    }
    gComboCtx.mmHintsRead[source >> 3] |= (uint8_t)(1u << (source & 7u));
    return true;
}

int Combo_HintReadCount(uint8_t game) {
    if (game != (uint8_t)GAME_MM) {
        return 0;
    }
    int n = 0;
    for (uint16_t s = 0; s < RSBS_MM_HINT_READ_CAP; s++) {
        n += Combo_HintIsRead(game, s) ? 1 : 0;
    }
    return n;
}

// ============================================================================
// The pane
// ============================================================================

/**
 * The MM save the pane reads and whether it holds a randomizer world: the
 * check tracker's MM source pick, so both panes switch source together.
 * NULL (UNAVAILABLE) without both adapters registered.
 */
static const void* MMHintSave(uint8_t* outFreshness, bool* outWorld) {
    uint8_t freshness = COMBO_TRACKER_FRESH_UNAVAILABLE;
    const void* save = NULL;
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    if (sMMHintRegistered && desc != NULL) {
        save = Combo_TrackerMMSave(&freshness);
    }
    bool world = false;
    if (save != NULL) {
        uint32_t saveType = 0;
        memcpy(&saveType, (const uint8_t*)save + desc->saveTypeOffset, sizeof(saveType));
        world = saveType == desc->saveTypeRando;
    }
    if (outFreshness != NULL) {
        *outFreshness = save != NULL ? freshness : (uint8_t)COMBO_TRACKER_FRESH_UNAVAILABLE;
    }
    if (outWorld != NULL) {
        *outWorld = world;
    }
    return save;
}

/** Whether MM check `check` is collected in the save image `save`, through the
 *  check tracker's MM descriptor (the obtained byte its rows read). */
static bool MMCheckObtained(const void* save, uint16_t check) {
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    if (save == NULL || desc == NULL || check >= desc->checkCount) {
        return false;
    }
    const uint8_t* row = (const uint8_t*)save + desc->checkTableOffset + (size_t)check * desc->checkStride;
    return row[desc->obtainedOffset] != 0;
}

/**
 * The pane's rows in order, as source ids, into `out` (RSBS_MM_HINT_READ_CAP
 * entries); returns how many. The world's enabled sources only, grouped by
 * the source's area (ascending), then by source id: an insertion sort over at
 * most 80 ids, stable by construction.
 */
static int MMHintOrder(const void* save, uint16_t* out) {
    int n = 0;
    for (uint16_t s = 0; s < sMMHintDesc.sourceCount && s < RSBS_MM_HINT_READ_CAP; s++) {
        ComboHintSourceInfo info;
        memset(&info, 0, sizeof(info));
        if (!sMMHintDesc.sourceInfo(s, &info) || !sMMHintDesc.listed(save, s)) {
            continue;
        }
        int at = n;
        while (at > 0) {
            ComboHintSourceInfo prev;
            memset(&prev, 0, sizeof(prev));
            (void)sMMHintDesc.sourceInfo(out[at - 1], &prev);
            if (prev.area.area <= info.area.area) {
                break;
            }
            out[at] = out[at - 1];
            at--;
        }
        out[at] = s;
        n++;
    }
    return n;
}

void Combo_HintGameSummary(uint8_t game, ComboHintGameSummary* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (game != (uint8_t)GAME_MM) {
        return; // OoT's pane is lane H2's (#1468's data)
    }
    bool world = false;
    const void* save = MMHintSave(&out->freshness, &world);
    out->hasWorld = world;
    if (save == NULL || !world) {
        return;
    }
    uint16_t order[RSBS_MM_HINT_READ_CAP];
    out->sourceCount = MMHintOrder(save, order);
    for (int i = 0; i < out->sourceCount; i++) {
        out->readCount += Combo_HintIsRead(game, order[i]) ? 1 : 0;
    }
}

int Combo_HintRowCount(uint8_t game) {
    if (game != (uint8_t)GAME_MM) {
        return 0;
    }
    bool world = false;
    const void* save = MMHintSave(NULL, &world);
    if (save == NULL || !world) {
        return 0;
    }
    uint16_t order[RSBS_MM_HINT_READ_CAP];
    return MMHintOrder(save, order);
}

bool Combo_HintRowAt(uint8_t game, int index, ComboHintRow* out) {
    if (out == NULL || index < 0) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (game != (uint8_t)GAME_MM) {
        return false;
    }
    bool world = false;
    const void* save = MMHintSave(NULL, &world);
    if (save == NULL || !world) {
        return false;
    }
    uint16_t order[RSBS_MM_HINT_READ_CAP];
    const int count = MMHintOrder(save, order);
    if (index >= count) {
        return false;
    }
    const uint16_t source = order[index];
    (void)sMMHintDesc.sourceInfo(source, &out->info);
    out->areaName = Combo_Area_Name(out->info.area);
    out->read = Combo_HintIsRead(game, source);
    // Masked until read: an unread row resolves nothing, so it carries nothing
    // the player has not been shown.
    if (out->read && sMMHintDesc.resolve(save, source, &out->hint)) {
        out->known = true;
        (void)Combo_HintPlainText(out->hint.text, out->display, (int)sizeof(out->display));
        out->found = out->hint.targetCount > 0;
        for (int i = 0; i < (int)out->hint.targetCount && i < RSBS_HINT_TARGET_CAP; i++) {
            const ComboHintTarget* t = &out->hint.targets[i];
            if (t->checkGame != (uint8_t)GAME_MM || !MMCheckObtained(save, t->check)) {
                out->found = false;
            }
        }
    } else {
        Combo_HintResolutionClear(&out->hint);
        snprintf(out->display, sizeof(out->display), "%s", RSBS_HINT_MASKED_TEXT);
    }
    return true;
}
