/**
 * @file combo_hint_view.h
 * @brief View model for the combined Hint Tracker's Majora's Mask pane: MM's
 *        hint sources, what each says, and which the player has read (#1475,
 *        lane H1; the window is lane H2's).
 *
 * WHAT PROBLEM THIS SOLVES. Majora's Mask has no hint objects: a gossip stone
 * works out what it says when it is talked to (EnGs.cpp: a stone seed from its
 * scene and position, the world's seed, a pick among the hintable checks), and
 * the fixed-location hints (the Clock Town posters, the Oath, the Hookshot, the
 * gold dust, the spider houses, the Great Fairies, the Bomb Shop's next stock)
 * build their text in their own textbox hooks. So nothing could list MM's hints,
 * say which were read, or show one while Ocarina of Time is played. This header
 * is that list, game-neutral, the way combo_tracker_view.h is the check list:
 *
 *   - THE SOURCES. Every MM hint source has a stable id (RSBS_MM_HINT_SRC_*
 *     below): the 58 gossip-stone placements in EnGs.cpp's kMmGossipStones order,
 *     then MM's 16 fixed-location hints. A stone placed in two scene versions
 *     (the Southern Swamp poisoned and cleared, Mountain Village in winter and
 *     spring) is a separate row of that table with its own scene id, so its own
 *     stone seed and its own hint: the two versions are two sources, keyed by
 *     (scene, x, z) like every other placement. Purchased hints are not sources
 *     (their pick is seeded by the frame counter).
 *   - THE RESOLVER. What a source says is a pure function of the world (the MM
 *     save's placement table, its final seed, the crossing store and the frozen
 *     Way of the Hero) and the source, computed by the MM TU (Rando/HintSources.h)
 *     with the very functions MM's textbox hooks call, so the pane's text is the
 *     textbox's text. It never touches MM's global random stream.
 *   - THE READ RECORD. One bit per source in the combo save record
 *     (ComboContext.mmHintsRead, ADR 0009 claim 20), set by MM's read hook when a
 *     source's hint textbox opens. It rides every .redsave write, survives every
 *     crossing, and starts empty for a new file.
 *   - THE PANE. Rows grouped by the MM area where the hint is found, masked
 *     ("???") until read, over the same MM save the check tracker's MM pane reads
 *     (Combo_TrackerMMSave: LIVE while MM is played, STALE from the frozen MM half
 *     otherwise, UNAVAILABLE with neither), so the pane renders while OoT runs.
 *
 * ADR 0002: every MM id here (a source's hinted check, its item) is game-local
 * and travels with its GameId; nothing compares ids across games.
 *
 * Locked by mm-hint-sources (ROM-free) and mm-hint-sources-world (rando tier),
 * src/common/tests/test_mm_hint_sources.c.
 */

#ifndef RSBS_COMMON_COMBO_HINT_VIEW_H
#define RSBS_COMMON_COMBO_HINT_VIEW_H

#include "combo_area.h"         // ComboArea
#include "combo_tracker_view.h" // ComboTrackerFreshness, the MM source pick, context.h

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Majora's Mask's hint sources (stable ids: the read record's bit indices)
// ============================================================================
//
// APPEND-ONLY: an id is a bit of every .redsave's read record, so a source never
// changes its id. The stones are EnGs.cpp's kMmGossipStones rows in table order
// (that table is the ROM's own En_Gs placements and is pinned at 58 rows).

enum {
    RSBS_MM_HINT_SRC_STONE_FIRST = 0,
    RSBS_MM_HINT_SRC_STONE_COUNT = 58,
    // The Clock Town "Recruiting Soldiers" posters (EnTalk.cpp, text 0x1C06, pages 1-4).
    RSBS_MM_HINT_SRC_REMAINS_ODOLWA = 58,
    RSBS_MM_HINT_SRC_REMAINS_GOHT = 59,
    RSBS_MM_HINT_SRC_REMAINS_GYORG = 60,
    RSBS_MM_HINT_SRC_REMAINS_TWINMOLD = 61,
    // The Skull Kid on the Clock Tower rooftop, once the remains are in (DmStk.cpp, 0x2013).
    RSBS_MM_HINT_SRC_OATH = 62,
    // The Zora who overheard the pirates (EnZow.cpp, 0x12F3 .. 0x1301).
    RSBS_MM_HINT_SRC_HOOKSHOT = 63,
    // The smithy's two gold dust lines (EnKgy.cpp, 0xC49 and 0xC4B).
    RSBS_MM_HINT_SRC_GOLD_DUST = 64,
    RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE = 65,
    // The spider houses' cursed men (EnSsh.cpp, 0x915; 0x1130 / 0x1131).
    RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP = 66,
    RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN = 67,
    // The Great Fairies (EnElfgrp.cpp, 0x578 / 0x580 and 0x582 .. 0x58C).
    RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN = 68,
    RSBS_MM_HINT_SRC_FAIRY_WOODFALL = 69,
    RSBS_MM_HINT_SRC_FAIRY_SNOWHEAD = 70,
    RSBS_MM_HINT_SRC_FAIRY_GREAT_BAY = 71,
    RSBS_MM_HINT_SRC_FAIRY_IKANA = 72,
    // The Bomb Shop's "we should be able to sell" line (EnGirlA.cpp, 0x648).
    RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK = 73,
    RSBS_MM_HINT_SRC_COUNT = 74
};

// ============================================================================
// What a source says (the resolver's answer)
// ============================================================================

/** What kind of hint a source gives. */
typedef enum {
    COMBO_HINT_TYPE_NONE = 0,    // nothing to say: no world, the hint is off, or no stone candidate
    COMBO_HINT_TYPE_ITEM = 1,    // a gossip stone: an item is at a check, in an area
    COMBO_HINT_TYPE_FOOLISH = 2, // a gossip stone: an area holds nothing important
    COMBO_HINT_TYPE_WOTH = 3,    // a gossip stone: an area is on the Way of the Hero
    COMBO_HINT_TYPE_FIXED = 4,   // a fixed-location hint: always about the same item or check(s)
} ComboHintType;

/** The textbox text's capacity, terminator included (MM's longest is the Clock Town fairy's, ~190). */
#define RSBS_HINT_TEXT_CAP 384
/** The most checks (and areas) one hint names: the Clock Town Great Fairy's two rewards. */
#define RSBS_HINT_TARGET_CAP 2

/**
 * One check a hint is about. `checkGame` is GAME_NONE when the hint names no
 * check (an area-only hint); the check id is that game's own (an OoT host for an
 * MM item that crossed). `itemGame`/`item` name what the check holds as the hint
 * presents it: the crossed item, never a crossing host's cover. `area` is the
 * check's area in the shared vocabulary, whether or not the text says it.
 */
typedef struct {
    uint8_t checkGame;
    uint16_t check;
    uint8_t itemGame;
    uint16_t item;
    ComboArea area;
} ComboHintTarget;

/**
 * A source's resolved hint. `text` is the textbox's message exactly as MM's
 * hook builds it, BEFORE colours and line breaks (MM's %r/%g/%y/%b/%p/%w colour
 * codes and its control bytes are still in it; Combo_HintPlainText strips them),
 * with the "(OoT)" marker on a crossed OoT item's name. `areas` are the areas the
 * TEXT names, in text order.
 */
typedef struct {
    uint8_t type; // ComboHintType
    uint8_t targetCount;
    ComboHintTarget targets[RSBS_HINT_TARGET_CAP];
    uint8_t areaCount;
    ComboArea areas[RSBS_HINT_TARGET_CAP];
    char text[RSBS_HINT_TEXT_CAP];
} ComboHintResolution;

/** Zero `r` (type NONE, no targets, no areas, empty text). NULL is ignored. */
void Combo_HintResolutionClear(ComboHintResolution* r);
/** Append `t` to `r`'s targets; false (nothing written) when full or NULL. */
bool Combo_HintResolutionAddTarget(ComboHintResolution* r, const ComboHintTarget* t);
/** Append `a` to the areas the text names; false when full, NULL, or `a` is no area of a game. */
bool Combo_HintResolutionAddArea(ComboHintResolution* r, ComboArea a);
/** Copy `text` into `r->text`, truncated to fit; false when it did not fit whole. */
bool Combo_HintResolutionSetText(ComboHintResolution* r, const char* text);

/**
 * `msg` as the pane prints it: MM's colour codes (%r %g %y %b %p %w) dropped,
 * every control byte (a line or box break, a sound or end code) turned into one
 * space, runs of spaces collapsed and the ends trimmed. Returns the full length
 * (snprintf's convention); `out` may be NULL with `cap` 0 to measure.
 */
int Combo_HintPlainText(const char* msg, char* out, int cap);

// ============================================================================
// Where a source is (static, per source)
// ============================================================================

typedef enum {
    COMBO_HINT_SOURCE_GOSSIP_STONE = 1,
    COMBO_HINT_SOURCE_FIXED = 2,
} ComboHintSourceKind;

typedef struct {
    uint16_t source;  // RSBS_MM_HINT_SRC_*
    uint8_t kind;     // ComboHintSourceKind
    ComboArea area;   // where the hint is read: the pane groups by it
    const char* name; // the row's label; static storage of the owning game's TU, never NULL once filled
} ComboHintSourceInfo;

// ============================================================================
// The MM adapter (registered by games/mm/2s2h/Rando/HintSourcesSingleExe.cpp)
// ============================================================================

typedef struct ComboMMHintDesc {
    uint16_t sourceCount; // RSBS_MM_HINT_SRC_COUNT
    // Static facts about `source`; false for an id past the count.
    bool (*sourceInfo)(uint16_t source, ComboHintSourceInfo* out);
    // Whether the world in the SaveContext image `save` gives this hint at all
    // (its option is on: RO_HINTS_GOSSIP_STONES for a stone, RO_HINTS_BOSS_REMAINS
    // for a poster, ...; the gold dust, fairy and Bomb Shop lines are always on).
    bool (*listed)(const void* save, uint16_t source);
    // What `source` says in the world of `save` (the live save or the frozen MM
    // half, whichever Combo_TrackerMMSave picked; MM's SaveContext layout). Pure:
    // no global random stream, no save written. False (out cleared) when the
    // image holds no randomizer world or the source says nothing there.
    bool (*resolve)(const void* save, uint16_t source, ComboHintResolution* out);
} ComboMMHintDesc;

/** Install MM's descriptor (copied). NULL un-registers. A descriptor with a NULL
 *  member or a source count past the read record's capacity is refused. */
void Combo_Hint_RegisterMM(const ComboMMHintDesc* desc);
/** The registered MM descriptor, or NULL. */
const ComboMMHintDesc* Combo_Hint_GetMMDesc(void);

/**
 * Build and register MM's descriptor. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/HintSourcesSingleExe.cpp); MM_TrackerAdapter_Register
 * calls it, so every path that brings up the combo tracker's MM adapter
 * (Combo_TrackerWindow_Init, which main.cpp calls) brings this one up too.
 */
void MM_HintAdapter_Register(void);

// ============================================================================
// The read record (ComboContext.mmHintsRead; ADR 0009 claim 20)
// ============================================================================

/** Whether `game`'s source `source` has been read. Only GAME_MM has a record (OoT's is SoH's, lane H2). */
bool Combo_HintIsRead(uint8_t game, uint16_t source);
/** Record that `game`'s source `source` was read; true when this call set it. Only GAME_MM. */
bool Combo_HintMarkRead(uint8_t game, uint16_t source);
/** How many of `game`'s sources are read. */
int Combo_HintReadCount(uint8_t game);

// ============================================================================
// The pane's reads (all null-safe; only GAME_MM answers in lane H1)
// ============================================================================

typedef struct {
    uint8_t freshness; // ComboTrackerFreshness of the MM save the pane reads
    bool hasWorld;     // that save holds a randomizer world
    int sourceCount;   // rows the pane lists (the world's enabled sources)
    int readCount;     // of those, read
} ComboHintGameSummary;

/** Fill `out` for `game`; UNAVAILABLE zeros when there is nothing to read. NULL `out` is ignored. */
void Combo_HintGameSummary(uint8_t game, ComboHintGameSummary* out);

/**
 * One row of the pane. `known` is whether `hint` holds the source's resolved
 * hint: only once the source is read and the world is readable, so an unread
 * row carries nothing the player has not seen. `display` is what the row
 * prints: "???" until known, then Combo_HintPlainText of the hint's text.
 * `areaName` is Combo_Area_Name(info.area), the group header.
 */
typedef struct {
    ComboHintSourceInfo info;
    const char* areaName;
    bool read;
    bool known;
    ComboHintResolution hint;
    char display[RSBS_HINT_TEXT_CAP];
} ComboHintRow;

/** The text an unread row prints. */
#define RSBS_HINT_MASKED_TEXT "???"

/** Rows `game`'s pane lists; 0 when UNAVAILABLE or the save holds no randomizer world. */
int Combo_HintRowCount(uint8_t game);

/**
 * Row `index`, in the pane's order: grouped by area (ascending area id of the
 * source's game's half), then by source id. False out of range / unavailable.
 */
bool Combo_HintRowAt(uint8_t game, int index, ComboHintRow* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_HINT_VIEW_H
