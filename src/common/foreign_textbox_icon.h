/**
 * @file foreign_textbox_icon.h
 * @brief The in-textbox icon of a cross-game item, answered by its ORIGIN game
 *        (#607, Tier 2b of #494).
 *
 * A foreign item picked up in a host game is presented as an ordinary pickup of
 * that game (#510): "You found the Hookshot!" in the host's own blue get-item
 * textbox. The host cannot name the icon itself — MM has no table that maps an
 * RG_* to anything (ADR 0002) — so, exactly as the name and the article travel
 * through the origin game's describer (foreign_items.h), the icon travels
 * through a source the ORIGIN game registers here.
 *
 * What travels is deliberately host-neutral: a resource path in the origin
 * game's own archive plus the SHAPE of the texture behind it. The host needs
 * the shape because its textbox renderer loads the icon with a fixed format and
 * size per branch (MM's MM_Message_DrawItemIcon draws RGBA32 32x32, RGBA32 24x24
 * or a tinted IA8 16x24 note, keyed on the message's item id); a path alone
 * would leave the host guessing how many texels to read. The origin never sees
 * the host's item enum either: the host chooses which of its own branches draws
 * each shape.
 *
 * 0 answers are part of the contract, never an error: an item the origin has no
 * textbox icon for, a game that registered no source, and an id the origin does
 * not know all answer 0, and the host keeps the icon-less textbox it showed
 * before #607.
 *
 * src/common includes no game header (ADR 0002); this header is plain C.
 */

#ifndef RSBS_COMMON_FOREIGN_TEXTBOX_ICON_H
#define RSBS_COMMON_FOREIGN_TEXTBOX_ICON_H

#include <stdint.h>

#include "context.h" // SharedItem

#ifdef __cplusplus
extern "C" {
#endif

/** The texel layout behind ComboTextboxIcon::texture. Values are stable: the
 *  test bridges and both games agree on them. */
typedef enum {
    COMBO_TEXTBOX_ICON_NONE = 0,
    /** RGBA32, 32x32: an ordinary inventory item icon (OoT icon_item_static). */
    COMBO_TEXTBOX_ICON_ITEM = 1,
    /** RGBA32, 24x24: a quest-status icon (OoT icon_item_24_static). */
    COMBO_TEXTBOX_ICON_QUEST = 2,
    /** IA8, 16x24: the song note, drawn tinted by r/g/b. */
    COMBO_TEXTBOX_ICON_NOTE = 3,
    /** IA8, 16x16: the rupee counter icon, drawn tinted by r/g/b. */
    COMBO_TEXTBOX_ICON_RUPEE = 4,
    COMBO_TEXTBOX_ICON_SHAPE_COUNT
} ComboTextboxIconShape;

typedef struct {
    /** "__OTR__..." resource path in the ORIGIN game's archives. Static storage:
     *  it outlives the textbox that keeps the bare pointer. */
    const char* texture;
    /** ComboTextboxIconShape. */
    uint8_t shape;
    /** Tint for the IA shapes (the origin's own colour for that icon); 255,255,255
     *  for the RGBA shapes, which draw untinted. */
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ComboTextboxIcon;

/** A game's answer for an item of its OWN id-space: 1 and *out filled, or 0
 *  (no icon). Must be a pure function of static tables: the host calls it
 *  mid-gameplay, while the origin game is suspended. */
typedef int (*ComboTextboxIconFn)(uint16_t id, ComboTextboxIcon* out);

/** Register (or, with NULL, un-register) `game`'s source. A non-game is ignored. */
void Combo_RegisterTextboxIconSource(uint8_t game, ComboTextboxIconFn source);

/** The registered source of `game`, or NULL (test observability). */
ComboTextboxIconFn Combo_GetTextboxIconSource(uint8_t game);

/**
 * The textbox icon for `item`, from its origin game's source: 1 and *out filled
 * with a non-empty texture and a known shape, or 0 and *out zeroed. The item's
 * own tag picks the source, so an OoT id is never answered from MM's tables.
 * Flags are ignored (a redeemed entry keeps its icon). A source that answers 1
 * with an empty texture or an unknown shape is overruled to 0 here, so a host
 * never draws a half-answer.
 */
int Combo_GetForeignItemTextboxIcon(SharedItem item, ComboTextboxIcon* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_FOREIGN_TEXTBOX_ICON_H
