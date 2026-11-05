/**
 * Rando::HintSources — Majora's Mask's hint sources for the combined Hint
 * Tracker (#1475, lane H1).
 *
 * Single-exe only. MM's hints are made where they are read: a gossip stone in
 * its textbox hook (ActorBehavior/EnGs.cpp), each fixed-location hint in its
 * own (EnTalk, DmStk, EnZow, EnKgy, EnSsh, EnElfgrp, EnGirlA). This is the one
 * place that says, for every source of src/common's combo_hint_view.h
 * (RSBS_MM_HINT_SRC_*), what it says in a given world, and it is the code those
 * hooks run: a hook builds its text with FixedText (or, for a stone, EnGs.cpp's
 * resolver functions), so the tracker's text and the textbox's cannot drift.
 *
 * THE WORLD IS AN ARGUMENT. Every text here is built from the save table it is
 * handed (`checks`, a SaveContext image's RANDO_SAVE_CHECKS) rather than from
 * gSaveContext: MM's live save at talk time, the frozen MM half the combined
 * tracker reads while Ocarina of Time is played. The resolver (Resolve,
 * ResolveStone, FixedText with JunkName::Plain) reads nothing else of MM's: no
 * global Ship_Random draw, no live save, no save written, no hint changed.
 *
 * THE ONE DELIBERATE DIFFERENCE (JunkName). MM names its own junk padding
 * (RI_JUNK) "Junk (<junk item>)" (Rando::StaticData::GetItemName), and the
 * suffix is Rando::CurrentJunkItem's pick: drawn from MM's global stream, checked
 * against MM's live save, cached for 20 frames. The only hint text that can name
 * it is the Bomb Shop's next-stock line, when its slot holds MM's own RI_JUNK
 * (junk filled there, or the check skipped or excluded). Its hook keeps main's
 * expression (JunkName::Textbox) so the textbox and the stream are byte for byte
 * main's; the resolver names that item "Junk" (JunkName::Plain), so the tracker
 * reads the same text without the frame's suffix. A slot whose OoT item crossed
 * also holds RI_JUNK, as a cover: both forms name the OoT item there, and the
 * resolver never asks GetItemName about the cover.
 *
 * THE READ HOOK. MarkRead is called by each hook as it shows a source's hint:
 * it sets the source's bit in the combo save record (ComboContext.mmHintsRead)
 * and marks every MM check the hint names Seen in MM's live save, as the Bomb
 * Shop's line already did (MarkCheckSeen), so the combined Check Tracker's MM
 * pane shows it. Naming an OoT host (a crossed MM item) is lane H2's.
 */
#ifndef RANDO_HINT_SOURCES_H
#define RANDO_HINT_SOURCES_H

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <string>

#include "2s2h/Rando/Rando.h" // SaveContext, RandoSaveCheck, RandoCheckId, RandoItemId

#include "combo_hint_view.h" // src/common: RSBS_MM_HINT_SRC_*, ComboHintResolution

namespace Rando {
namespace HintSources {

// ---- The gossip stones (defined in ActorBehavior/EnGs.cpp, beside the stone table) ----

/** kMmGossipStones' row count (RSBS_MM_HINT_SRC_STONE_COUNT). */
int StoneCount();

/** Row `index` of kMmGossipStones: its scene and home x / z. False out of range. */
bool StoneAt(int index, int16_t* outScene, int16_t* outX, int16_t* outZ);

/**
 * What the free stone placed at row `index` says in the world (`checks`,
 * `finalSeed`): the pick GetRandomCheck makes (the same candidates, the same
 * stream, the same draw, FreeStonePickFrom) and the text FreeStoneHintText shows,
 * with its type, target and areas. Pure: MM's global stream is never touched.
 * False (out cleared) when the stone has no candidate to name (its hook then
 * shows nothing) or `index` is out of range.
 */
bool ResolveStone(const RandoSaveCheck* checks, uint32_t finalSeed, int index, ComboHintResolution* out);

// ---- The fixed-location hints (HintSourcesSingleExe.cpp) ----

/** How FixedText names MM's own junk padding (RI_JUNK); see THE ONE DELIBERATE
 *  DIFFERENCE above. Only the Bomb Shop's next-stock line can name it. */
enum class JunkName : uint8_t {
    // "Junk": a pure function of the world. The resolver's, and every hook's but
    // the Bomb Shop's.
    Plain,
    // GetItemName's "Junk (<junk item>)", the suffix drawn by
    // Rando::CurrentJunkItem from MM's global stream: the Bomb Shop hook's,
    // main's expression call for call.
    Textbox,
};

/**
 * The message fixed source `source` (RSBS_MM_HINT_SRC_REMAINS_ODOLWA ..
 * RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK) shows, built from `checks` exactly as its
 * textbox hook builds it (before colours and line breaks), and, when `out` is
 * non-NULL, the resolution (type FIXED, the item(s) and check(s) it names, the
 * areas its text names, the text). "" for a source that is not fixed. With
 * `junkName` Plain (the default) it is pure; Textbox is the Bomb Shop hook's form.
 */
std::string FixedText(uint16_t source, const RandoSaveCheck* checks, ComboHintResolution* out,
                      JunkName junkName = JunkName::Plain);

/** The item fixed source `source` locates (a remains poster's remains, the Oath's
 *  song, ...); RI_UNKNOWN when it names a check's item instead, or is not fixed. */
RandoItemId FixedItem(uint16_t source);

/** The Great Fairy source for a fairy's reward check (RC_*_GREAT_FAIRY), or
 *  RSBS_MM_HINT_SRC_COUNT for any other check. */
uint16_t GreatFairySource(RandoCheckId randoCheckId);

// ---- Every source ----

/**
 * What `source` says in the SaveContext image `save` (MM's layout: the live
 * save or the frozen MM half). False (out cleared) when the image holds no
 * randomizer world, the source is off in it (Listed), or it says nothing.
 */
bool Resolve(const SaveContext* save, uint16_t source, ComboHintResolution* out);

/** Whether the world in `save` gives `source`'s hint at all (its option is on). */
bool Listed(const SaveContext* save, uint16_t source);

/**
 * THE READ HOOK: MM is showing `source`'s hint textbox now. Sets its bit in the
 * combo save record and marks every MM check the hint names Seen in MM's live
 * save. A source past the count is ignored.
 */
void MarkRead(uint16_t source);

} // namespace HintSources
} // namespace Rando

#endif // RSBS_SINGLE_EXECUTABLE

#endif // RANDO_HINT_SOURCES_H
