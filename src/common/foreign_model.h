/**
 * @file foreign_model.h
 * @brief The get-item MODEL of a cross-game item, answered by its ORIGIN game
 *        (#577 M2: the foreign model descriptor registry).
 *
 * Operator ruling (2026-09-30): foreign items get real 3D models in both games.
 * The host cannot name the model itself — it has no table that maps the other
 * game's item ids to anything (ADR 0002) — so, exactly as the name travels
 * through the origin's describer (foreign_items.h) and the textbox icon through
 * foreign_textbox_icon.h, the model travels through a source the ORIGIN game
 * registers here.
 *
 * WHAT TRAVELS. A host-neutral DESCRIPTOR: the origin's display lists by
 * resource path, plus the shape its own get-item draw function gives them (which
 * layer each list goes to, in which order, which setup list, the colours, the
 * texture scroll, the extra scale and rotation, the camera-facing parts). The
 * display lists are named by path (the Fast3D interpreter resolves "__OTR__..."
 * strings) and the descriptor carries nothing but paths and a shape. That is
 * enough only as far as the lists never read segment 6: OoT's get-item cutscene
 * still DMAs the gi object into `giObjectSegment` and binds segment 6 on both
 * layers before it draws (ovl_player_actor/z_player.c, z_player_lib.c
 * OoT_Player_DrawGetItemImpl), and 2S2H binds an empty scratch there. That the
 * extracted gi lists never read segment 6 is UNVERIFIED for every list (the
 * epic's Risks); the first consumer (M3 / M4) confirms it for what it draws.
 * The origin's draw FUNCTIONS are never called by the other game: they take the
 * origin's own PlayState layout and bind scrolling textures through the
 * origin's own graph allocator. The host re-expresses the shape with its own
 * primitives instead (M3 and M4 are the first consumers; this header has none
 * yet).
 *
 * THREE ANSWERS (ComboModelAnswerKind), never an error:
 *  - DESCRIPTOR: the origin's model, drawable only IF its paths resolve. The
 *    answer is a pure function of static tables and says nothing about which
 *    archives are mounted: MM's archives are added only when MM is first
 *    entered, OoT's from OoT's first boot (rsbs/src/main.cpp,
 *    Combo_EnsureGameArchivesLoaded). An OoT session that has not entered MM in
 *    this process therefore gets a DESCRIPTOR for an MM-exclusive model (the
 *    Deku Mask) whose paths cannot resolve, and an MM-first session the same
 *    for an OoT model. The CONSUMER owns that check: before it draws, every
 *    part's path must resolve (ArchiveManager::HasFile, as
 *    ForeignTextboxIconSingleExe.cpp does for the textbox icon); otherwise it
 *    keeps today's stand-in, as for NONE.
 *  - HOST_NATIVE: a part of the origin's model lives in an object directory BOTH
 *    games' archives carry (`object_gi_hookshot`, `object_gi_rupy`, ...). The
 *    archives resolve one flat, last-added-wins path map and the resource cache
 *    keeps whichever copy loaded first, so the origin's path would draw the
 *    host's asset, or a mix of the two. The operator ruled host-native mapping
 *    first (what OoTMM does: when OoT draws MM's hookshot it draws OoT's
 *    hookshot): the HOST's registered mapper names its own equivalent model by
 *    an opaque key in its own draw space (`hostKey`). The mapping table itself
 *    is #577 M7's data.
 *  - NONE: no model. The host keeps today's stand-in. An origin draw the
 *    descriptor cannot express, a colliding model the host has no mapping for,
 *    an unknown id, an untagged item, a native item and a half-answer all land
 *    here.
 *
 * src/common includes no game header (ADR 0002); this header is plain C. No raw
 * game id crosses it except the item's own SharedItem tag and the host's opaque
 * `hostKey`, which only ever goes back to the host that minted it.
 */

#ifndef RSBS_COMMON_FOREIGN_MODEL_H
#define RSBS_COMMON_FOREIGN_MODEL_H

#include <stdint.h>

#include "context.h" // SharedItem

#ifdef __cplusplus
extern "C" {
#endif

/** Most display lists any get-item draw in either game emits (OoT's wallet: 8). */
#define COMBO_MODEL_MAX_PARTS 8
/** Most scrolling-texture segments one get-item draw binds (OoT's jewels: 2). */
#define COMBO_MODEL_MAX_SCROLLS 2

/** Values are stable: both games and the test bridges agree on them. */
typedef enum {
    COMBO_MODEL_LAYER_NONE = 0,
    COMBO_MODEL_LAYER_OPA = 1,
    COMBO_MODEL_LAYER_XLU = 2,
} ComboModelLayer;

typedef enum {
    COMBO_MODEL_ANSWER_NONE = 0,
    COMBO_MODEL_ANSWER_DESCRIPTOR = 1,
    COMBO_MODEL_ANSWER_HOST_NATIVE = 2,
} ComboModelAnswerKind;

typedef struct {
    /** "__OTR__objects/<objectDir>/<name>" in the ORIGIN game's archives. Static
     *  storage: it outlives every frame that draws it. */
    const char* dl;
    /** ComboModelLayer. */
    uint8_t layer;
    /** 1: drawn under the camera-facing rotation (after `billboardOffset`), the
     *  way the origin draws a flame or a bottled spirit. */
    uint8_t billboard;
} ComboModelPart;

/**
 * A scrolling texture the origin binds to `segment` before drawing `layer`:
 * tile coordinate = base + perFrame * frames, the arguments both games'
 * TexScroll / TwoTexScroll take. An unused slot has segment 0.
 */
typedef struct {
    uint8_t segment;
    uint8_t layer;
    /** 0: one tile (TexScroll with x1/y1/w1/h1); 1: two tiles (TwoTexScroll). */
    uint8_t twoTiles;
    int16_t x1;
    int16_t y1;
    int16_t x1PerFrame;
    int16_t y1PerFrame;
    uint16_t w1;
    uint16_t h1;
    int16_t x2;
    int16_t y2;
    int16_t x2PerFrame;
    int16_t y2PerFrame;
    uint16_t w2;
    uint16_t h2;
} ComboModelScroll;

/** Colours the origin sets on a layer before its lists; `set` 0 leaves the
 *  layer's colours to the lists themselves. */
typedef struct {
    uint8_t set;
    uint8_t primLodFrac;
    uint8_t prim[3];
    uint8_t env[3];
} ComboModelColor;

typedef struct {
    /** Parts in the origin's emission order (order matters within a layer). */
    uint8_t partCount;
    ComboModelPart parts[COMBO_MODEL_MAX_PARTS];
    /** The SETUPDL index each layer is prepared with (25 in most draws; OoT's
     *  medallions and masks use 26, compasses and "sold out" 5, MM's bombchu
     *  23). Both ports inherit the same setup-list table. 0 when the layer has
     *  no part. */
    uint8_t opaSetupDl;
    uint8_t xluSetupDl;
    ComboModelScroll scrolls[COMBO_MODEL_MAX_SCROLLS];
    ComboModelColor opaColor;
    ComboModelColor xluColor;
    /** 1: every part is drawn tinted by `grayscaleRgb` (OoT's generic song
     *  notes). */
    uint8_t grayscale;
    uint8_t grayscaleRgb[3];
    /** Uniform scale on top of the host's own get-item scale; 1.0f for none
     *  (OoT's small rupees draw at 0.7). */
    float scale;
    /** Rotation applied after the scale, as Matrix_RotateZYX binary angles
     *  (x, y, z); all 0 for none (OoT's spiritual stones stand upright). */
    int16_t rotation[3];
    /** Translation applied before the camera-facing rotation of the billboard
     *  parts (OoT's blue fire sits off-centre). */
    float billboardOffset[3];
} ComboModel;

typedef struct {
    /** ComboModelAnswerKind. */
    uint8_t kind;
    /** HOST_NATIVE only: the host's own model, in the host's own draw space,
     *  as its mapper named it. Meaningless to anyone but that host. */
    uint16_t hostKey;
    /** DESCRIPTOR: the model to draw. HOST_NATIVE: the origin's model the host
     *  mapped from. NONE: zeroed. */
    ComboModel model;
} ComboModelAnswer;

/** A game's answer for an item of its OWN id-space: 1 and *out a descriptor, or
 *  0 (no model). Must be a pure function of static tables: the host calls it
 *  mid-gameplay, while the origin game is suspended. */
typedef int (*ComboModelSourceFn)(uint16_t id, ComboModel* out);

/** A HOST's mapping for a foreign model that collides with its own archive: 1
 *  and *hostKey its own equivalent, or 0 (no mapping: no model). Pure, static. */
typedef int (*ComboHostNativeModelFn)(const ComboModel* foreign, uint16_t* hostKey);

/**
 * One row of a HOST's host-native table (#577 M7): a colliding foreign model,
 * named by its parts, and what the host draws for it instead. Each game keeps
 * its own table beside its mapper; this type and Combo_HostNativeFind are the
 * shared lookup, so both games key their rows the same way.
 *
 * The key is the foreign model's WHOLE part list, in the origin's emission
 * order, each part written "<objectDir>/<name>" (the "__OTR__objects/" prefix
 * every part carries is implied). A first list alone does not identify a
 * model: both games' heart container and heart piece start with the same
 * border list, and their magic arrows with the same shaft.
 */
typedef struct {
    /** The foreign parts; unused slots are NULL, and a key has at least one. */
    const char* foreignParts[COMBO_MODEL_MAX_PARTS];
    /** >= 0: the HOST's own get-item draw row (its hostKey). -1: no model. */
    int16_t hostDrawId;
    /** hostDrawId -1: why the host draws no model. NULL otherwise. */
    const char* noModelReason;
} ComboHostNativeRow;

/** The index of the row of `rows` whose key is exactly `foreign`'s part list,
 *  or -1 (no row, or a NULL / empty argument). */
int Combo_HostNativeFind(const ComboHostNativeRow* rows, int count, const ComboModel* foreign);

/** Register (or, with NULL, un-register) `game`'s source. A non-game is ignored. */
void Combo_RegisterModelSource(uint8_t game, ComboModelSourceFn source);
/** The registered source of `game`, or NULL (test observability). */
ComboModelSourceFn Combo_GetModelSource(uint8_t game);

/** Register (or un-register) the host-native mapper `hostGame` draws colliding
 *  foreign models with. A non-game is ignored. */
void Combo_RegisterHostNativeModel(uint8_t hostGame, ComboHostNativeModelFn mapper);
/** The registered mapper of `hostGame`, or NULL (test observability). */
ComboHostNativeModelFn Combo_GetHostNativeModel(uint8_t hostGame);

/** A model with no parts, scale 1: the starting point a source fills. */
void Combo_ModelInit(ComboModel* model);
/** Append a part; 0 (model unchanged) when it is full or the part is malformed. */
int Combo_ModelAddPart(ComboModel* model, const char* dl, uint8_t layer, uint8_t billboard);

/** 1 when every field is something a host can draw: at least one part, every
 *  part a known layer with an "__OTR__objects/<dir>/<name>" path, a setup list
 *  for exactly the layers in use, well-formed scrolls, a finite positive scale. */
int Combo_ModelIsWellFormed(const ComboModel* model);

/** 1 when an object directory of that name exists in BOTH games' archives
 *  (vanilla and the ports' custom assets), so a path under it resolves to
 *  whichever archive loaded last. The table is the in-tree intersection; the
 *  ForeignModel row re-derives it from both asset trees. */
int Combo_ForeignModel_ObjectDirCollides(const char* objectDir);
/** Combo_ForeignModel_ObjectDirCollides of the directory a display-list path
 *  names; 0 for a path that names no object directory. */
int Combo_ForeignModel_PathCollides(const char* dl);
/** Size of the collision table (test observability). */
int Combo_ForeignModel_CollidingDirCount(void);

/**
 * How `hostGame` draws a model its origin answered: DESCRIPTOR when no part
 * collides, HOST_NATIVE when one does and the host's mapper names an
 * equivalent, NONE otherwise (including a model that is not well-formed or a
 * non-game host). `out` is always written when non-NULL. DESCRIPTOR does not
 * mean the origin's archive is mounted: the caller checks (THREE ANSWERS).
 */
uint8_t Combo_ClassifyForeignModel(uint8_t hostGame, const ComboModel* model, ComboModelAnswer* out);

/**
 * The model `hostGame` draws for `item`: the item's origin source's answer,
 * classified for the host (Combo_ClassifyForeignModel). The item's own tag picks
 * the source, so an OoT id is never answered from MM's tables; flags are ignored.
 * An item native to the host, an untagged item and a non-game host answer NONE.
 */
uint8_t Combo_GetForeignItemModel(uint8_t hostGame, SharedItem item, ComboModelAnswer* out);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_FOREIGN_MODEL_H
