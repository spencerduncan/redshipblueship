/**
 * @file foreign_model.c
 * @brief Registry for the origin-answered get-item model of a cross-game item
 *        (#577 M2). See foreign_model.h.
 */

#include "foreign_model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

// Indexed by GameId; GAME_NONE's slot is never written.
static ComboModelSourceFn sModelSources[3];
static ComboHostNativeModelFn sHostNativeModels[3];

static int ModelSlotValid(uint8_t game) {
    return game == (uint8_t)GAME_OOT || game == (uint8_t)GAME_MM;
}

void Combo_RegisterModelSource(uint8_t game, ComboModelSourceFn source) {
    if (ModelSlotValid(game)) {
        sModelSources[game] = source;
    }
}

ComboModelSourceFn Combo_GetModelSource(uint8_t game) {
    return ModelSlotValid(game) ? sModelSources[game] : NULL;
}

void Combo_RegisterHostNativeModel(uint8_t hostGame, ComboHostNativeModelFn mapper) {
    if (ModelSlotValid(hostGame)) {
        sHostNativeModels[hostGame] = mapper;
    }
}

ComboHostNativeModelFn Combo_GetHostNativeModel(uint8_t hostGame) {
    return ModelSlotValid(hostGame) ? sHostNativeModels[hostGame] : NULL;
}

// ---- Paths ------------------------------------------------------------------

static const char kOtrPrefix[] = "__OTR__";
static const char kObjectsPrefix[] = "objects/";

/** The object directory a display-list path names, as [*dir, *dir + *len), or 0
 *  for anything but "__OTR__objects/<dir>/<name>" with both parts non-empty. */
static int ObjectDirOfPath(const char* dl, const char** dir, size_t* len) {
    if (dl == NULL || strncmp(dl, kOtrPrefix, sizeof(kOtrPrefix) - 1) != 0) {
        return 0;
    }
    const char* rest = dl + sizeof(kOtrPrefix) - 1;
    if (strncmp(rest, kObjectsPrefix, sizeof(kObjectsPrefix) - 1) != 0) {
        return 0;
    }
    rest += sizeof(kObjectsPrefix) - 1;
    const char* slash = strchr(rest, '/');
    if (slash == NULL || slash == rest || slash[1] == '\0' || strchr(slash + 1, '/') != NULL) {
        return 0;
    }
    *dir = rest;
    *len = (size_t)(slash - rest);
    return 1;
}

// ---- The collision table ----------------------------------------------------
//
// Every object directory present in BOTH games' asset trees: games/oot/assets/
// objects + games/oot/assets/custom/objects (oot.o2r, soh.o2r) against
// games/mm/assets/objects + games/mm/assets/custom/objects (mm.o2r, 2ship.o2r).
// Sorted by strcmp (binary search below). The ForeignModel row re-derives the
// intersection from both trees and fails on any difference, so an asset tree
// change cannot leave this table stale. 151 are the audit's vanilla collisions
// (docs/resource-namespace-audit.md); the other 10 are the ports' custom
// objects both ship (the Triforce pieces, the ocarina buttons, ...).
static const char* const kCollidingObjectDirs[] = {
    "gameplay_dangeon_keep",
    "gameplay_field_keep",
    "gameplay_keep",
    "object_ahg",
    "object_am",
    "object_ani",
    "object_aob",
    "object_b_heart",
    "object_bba",
    "object_bdoor",
    "object_bg",
    "object_bigokuta",
    "object_bji",
    "object_bob",
    "object_boj",
    "object_bombf",
    "object_bombiwa",
    "object_box",
    "object_bubble",
    "object_cne",
    "object_cow",
    "object_crow",
    "object_cs",
    "object_d_hsblock",
    "object_d_lift",
    "object_daiku",
    "object_dekubaba",
    "object_dekunuts",
    "object_dnk",
    "object_dns",
    "object_dodongo",
    "object_dog",
    "object_ds2",
    "object_dy_obj",
    "object_efc_star_field",
    "object_efc_tw",
    "object_firefly",
    "object_fish",
    "object_fr",
    "object_fu",
    "object_fz",
    "object_ge1",
    "object_geldb",
    "object_gi_arrow",
    "object_gi_arrowcase",
    "object_gi_bean",
    "object_gi_bomb_1",
    "object_gi_bomb_2",
    "object_gi_bombpouch",
    "object_gi_bosskey",
    "object_gi_bottle",
    "object_gi_bow",
    "object_gi_compass",
    "object_gi_fish",
    "object_gi_ghost",
    "object_gi_glasses",
    "object_gi_golonmask",
    "object_gi_heart",
    "object_gi_hearts",
    "object_gi_hookshot",
    "object_gi_insect",
    "object_gi_key",
    "object_gi_ki_tan_mask",
    "object_gi_liquid",
    "object_gi_longsword",
    "object_gi_m_arrow",
    "object_gi_magicpot",
    "object_gi_map",
    "object_gi_melody",
    "object_gi_milk",
    "object_gi_nuts",
    "object_gi_ocarina",
    "object_gi_purse",
    "object_gi_rabit_mask",
    "object_gi_rupy",
    "object_gi_shield_2",
    "object_gi_shield_3",
    "object_gi_soldout",
    "object_gi_soul",
    "object_gi_stick",
    "object_gi_sutaru",
    "object_gi_sword_1",
    "object_gi_truth_mask",
    "object_gi_zoramask",
    "object_gla",
    "object_gm",
    "object_goroiwa",
    "object_gs",
    "object_hata",
    "object_hintnuts",
    "object_horse_link_child",
    "object_hs",
    "object_ik",
    "object_in",
    "object_js",
    "object_ka",
    "object_kanban",
    "object_kibako",
    "object_kibako2",
    "object_kusa",
    "object_kz",
    "object_lightswitch",
    "object_link_boy",
    "object_link_child",
    "object_ma1",
    "object_ma2",
    "object_mag",
    "object_mamenoki",
    "object_mastergolon",
    "object_masterzoora",
    "object_mir_ray",
    "object_mk",
    "object_mm",
    "object_ms",
    "object_mu",
    "object_nb",
    "object_niw",
    "object_nwc",
    "object_ny",
    "object_oF1d_map",
    "object_ocarina_a_button",
    "object_ocarina_c_down_button",
    "object_ocarina_c_left_button",
    "object_ocarina_c_right_button",
    "object_ocarina_c_up_button",
    "object_okuta",
    "object_os_anime",
    "object_owl",
    "object_po_composer",
    "object_po_sisters",
    "object_ps",
    "object_rd",
    "object_rr",
    "object_ru2",
    "object_sb",
    "object_skb",
    "object_spot11_obj",
    "object_ssh",
    "object_st",
    "object_stream",
    "object_syokudai",
    "object_tite",
    "object_tk",
    "object_toryo",
    "object_trap",
    "object_triforce_completed",
    "object_triforce_piece_0",
    "object_triforce_piece_1",
    "object_triforce_piece_2",
    "object_tsubo",
    "object_umajump",
    "object_vm",
    "object_wallmaster",
    "object_warp1",
    "object_wf",
    "object_wood02",
    "object_yabusame_point",
    "object_zg",
    "object_zl1",
    "object_zl4",
    "object_zo",
};

static int CompareDirKey(const void* key, const void* entry) {
    return strcmp((const char*)key, *(const char* const*)entry);
}

int Combo_ForeignModel_ObjectDirCollides(const char* objectDir) {
    if (objectDir == NULL || objectDir[0] == '\0') {
        return 0;
    }
    return bsearch(objectDir, kCollidingObjectDirs, sizeof(kCollidingObjectDirs) / sizeof(kCollidingObjectDirs[0]),
                   sizeof(kCollidingObjectDirs[0]), CompareDirKey) != NULL
               ? 1
               : 0;
}

// ---- Collision-free paths in colliding directories (#964) --------------------
//
// A directory both trees carry collides as a whole (the table above), but a
// resource NAME only one game's tree carries is not shadowed by the other game:
// the curated half carries it as a single-path manifest entry (both generators
// refuse a path any of the host's base archives carries), so it resolves to the
// origin's one copy whichever archive was added last. These are the display
// lists such entries let a host draw, as "<dir>/<name>", sorted by strcmp. The
// ForeignModel row (M17) checks the stray fairy's against both asset trees; an
// entry for another model needs the same check.
static const char* const kCollisionFreePaths[] = {
    "gameplay_keep/gStrayFairyGlowDL",           "gameplay_keep/gStrayFairyLeftArmDL",
    "gameplay_keep/gStrayFairyLeftFacingHeadDL", "gameplay_keep/gStrayFairyLeftWingDL",
    "gameplay_keep/gStrayFairyPelvisAndLegsDL",  "gameplay_keep/gStrayFairyRightArmDL",
    "gameplay_keep/gStrayFairyRightWingDL",      "gameplay_keep/gStrayFairyTorsoDL",
};

int Combo_ForeignModel_PathCollides(const char* dl) {
    const char* dir = NULL;
    size_t len = 0;
    char name[128];
    if (!ObjectDirOfPath(dl, &dir, &len) || len >= sizeof(name)) {
        return 0;
    }
    memcpy(name, dir, len);
    name[len] = '\0';
    if (!Combo_ForeignModel_ObjectDirCollides(name)) {
        return 0;
    }
    // `dir` runs on to the end of the path: "<dir>/<name>".
    return bsearch(dir, kCollisionFreePaths, sizeof(kCollisionFreePaths) / sizeof(kCollisionFreePaths[0]),
                   sizeof(kCollisionFreePaths[0]), CompareDirKey) != NULL
               ? 0
               : 1;
}

int Combo_ForeignModel_CollidingDirCount(void) {
    return (int)(sizeof(kCollidingObjectDirs) / sizeof(kCollidingObjectDirs[0]));
}

// ---- Descriptors ------------------------------------------------------------

void Combo_ModelInit(ComboModel* model) {
    if (model == NULL) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->scale = 1.0f;
}

static int LayerValid(uint8_t layer) {
    return layer == (uint8_t)COMBO_MODEL_LAYER_OPA || layer == (uint8_t)COMBO_MODEL_LAYER_XLU;
}

static int PartWellFormed(const ComboModelPart* part) {
    const char* dir = NULL;
    size_t len = 0;
    return LayerValid(part->layer) && part->billboard <= 1 && ObjectDirOfPath(part->dl, &dir, &len);
}

int Combo_ModelAddPart(ComboModel* model, const char* dl, uint8_t layer, uint8_t billboard) {
    if (model == NULL || model->partCount >= COMBO_MODEL_MAX_PARTS) {
        return 0;
    }
    ComboModelPart part;
    memset(&part, 0, sizeof(part));
    part.dl = dl;
    part.layer = layer;
    part.billboard = billboard;
    if (!PartWellFormed(&part)) {
        return 0;
    }
    model->parts[model->partCount++] = part;
    return 1;
}

static int ScrollWellFormed(const ComboModelScroll* scroll, int hasOpa, int hasXlu) {
    if (scroll->segment == 0) {
        // An unused slot is all zero, so a half-filled one cannot hide in it.
        static const ComboModelScroll kUnused;
        return memcmp(scroll, &kUnused, sizeof(kUnused)) == 0;
    }
    // Segments 8-0xD are the actor segments a get-item draw may bind; the rest
    // belong to the engine (0-7) or do not exist.
    if (scroll->segment < 8 || scroll->segment > 0xD || scroll->twoTiles > 1 || !LayerValid(scroll->layer)) {
        return 0;
    }
    if ((scroll->layer == (uint8_t)COMBO_MODEL_LAYER_OPA && !hasOpa) ||
        (scroll->layer == (uint8_t)COMBO_MODEL_LAYER_XLU && !hasXlu)) {
        return 0;
    }
    if (scroll->w1 == 0 || scroll->h1 == 0) {
        return 0;
    }
    if (scroll->twoTiles) {
        return scroll->w2 != 0 && scroll->h2 != 0;
    }
    return scroll->x2 == 0 && scroll->y2 == 0 && scroll->x2PerFrame == 0 && scroll->y2PerFrame == 0 &&
           scroll->w2 == 0 && scroll->h2 == 0;
}

// ---- Segment colours and joints (#964) ---------------------------------------

static int ActorSegment(uint8_t segment) {
    return segment >= 8 && segment <= 0xD;
}

static int SegmentColorUnused(const ComboModelSegmentColor* c) {
    static const uint8_t kZero[4];
    return c->segment == 0 && c->layer == 0 && c->primLodFrac == 0 && memcmp(c->prim, kZero, 4) == 0 &&
           memcmp(c->env, kZero, 4) == 0;
}

static int JointUnused(const ComboModelJoint* j) {
    return j->parent == 0 && j->pos[0] == 0 && j->pos[1] == 0 && j->pos[2] == 0 && j->rot[0] == 0 && j->rot[1] == 0 &&
           j->rot[2] == 0;
}

/** The joints form a forest whose parents come first, every joint a part names
 *  exists, some part uses the joints, and the joint segment and each segment
 *  colour name an actor segment that no scroll, other colour or the joints
 *  already fill on that layer. */
static int PoseWellFormed(const ComboModel* model, int hasOpa, int hasXlu) {
    if (model->jointCount > COMBO_MODEL_MAX_JOINTS) {
        return 0;
    }
    for (int i = 0; i < COMBO_MODEL_MAX_JOINTS; i++) {
        const ComboModelJoint* j = &model->joints[i];
        if (i >= (int)model->jointCount ? !JointUnused(j) : (j->parent < -1 || j->parent >= i)) {
            return 0;
        }
    }
    int usesJoint = 0;
    for (uint8_t i = 0; i < model->partCount; i++) {
        if (model->parts[i].joint > model->jointCount) {
            return 0;
        }
        usesJoint |= model->parts[i].joint != 0;
    }
    if ((model->jointCount != 0) != usesJoint) {
        return 0;
    }
    if (model->jointSegment != 0 && (!ActorSegment(model->jointSegment) || model->jointCount == 0)) {
        return 0;
    }
    for (int i = 0; i < COMBO_MODEL_MAX_SCROLLS; i++) {
        if (model->scrolls[i].segment != 0 && model->scrolls[i].segment == model->jointSegment) {
            return 0;
        }
    }
    for (int i = 0; i < COMBO_MODEL_MAX_SEGMENT_COLORS; i++) {
        const ComboModelSegmentColor* c = &model->segmentColors[i];
        if (c->segment == 0) {
            if (!SegmentColorUnused(c)) {
                return 0;
            }
            continue;
        }
        if (!ActorSegment(c->segment) || !LayerValid(c->layer) || c->segment == model->jointSegment) {
            return 0;
        }
        if ((c->layer == (uint8_t)COMBO_MODEL_LAYER_OPA && !hasOpa) ||
            (c->layer == (uint8_t)COMBO_MODEL_LAYER_XLU && !hasXlu)) {
            return 0;
        }
        for (int k = 0; k < COMBO_MODEL_MAX_SCROLLS; k++) {
            if (model->scrolls[k].segment == c->segment && model->scrolls[k].layer == c->layer) {
                return 0;
            }
        }
        for (int k = 0; k < i; k++) {
            if (model->segmentColors[k].segment == c->segment && model->segmentColors[k].layer == c->layer) {
                return 0;
            }
        }
    }
    return 1;
}

int Combo_ModelIsWellFormed(const ComboModel* model) {
    if (model == NULL || model->partCount == 0 || model->partCount > COMBO_MODEL_MAX_PARTS) {
        return 0;
    }
    int hasOpa = 0;
    int hasXlu = 0;
    for (uint8_t i = 0; i < model->partCount; i++) {
        if (!PartWellFormed(&model->parts[i])) {
            return 0;
        }
        hasOpa |= model->parts[i].layer == (uint8_t)COMBO_MODEL_LAYER_OPA;
        hasXlu |= model->parts[i].layer == (uint8_t)COMBO_MODEL_LAYER_XLU;
    }
    // A setup list for exactly the layers in use: a host must never guess how
    // to prepare a layer, nor prepare one nothing draws into.
    if ((hasOpa != (model->opaSetupDl != 0)) || (hasXlu != (model->xluSetupDl != 0))) {
        return 0;
    }
    // ... and the origin's own list for it (#1171): the two games' setup tables
    // differ, so a host never resolves the index in its own.
    if ((hasOpa != (model->opaSetupList != NULL)) || (hasXlu != (model->xluSetupList != NULL))) {
        return 0;
    }
    if (model->opaColor.set > 1 || model->xluColor.set > 1 || model->grayscale > 1) {
        return 0;
    }
    if ((model->opaColor.set && !hasOpa) || (model->xluColor.set && !hasXlu)) {
        return 0;
    }
    for (int i = 0; i < COMBO_MODEL_MAX_SCROLLS; i++) {
        if (!ScrollWellFormed(&model->scrolls[i], hasOpa, hasXlu)) {
            return 0;
        }
    }
    if (!isfinite(model->scale) || model->scale <= 0.0f) {
        return 0;
    }
    for (int i = 0; i < 3; i++) {
        if (!isfinite(model->billboardOffset[i])) {
            return 0;
        }
    }
    return PoseWellFormed(model, hasOpa, hasXlu);
}

// ---- Host-native tables (#577 M7) -------------------------------------------

static int HostNativeRowMatches(const ComboHostNativeRow* row, const ComboModel* foreign) {
    static const char kPartPrefix[] = "__OTR__objects/";
    int keyParts = 0;
    while (keyParts < COMBO_MODEL_MAX_PARTS && row->foreignParts[keyParts] != NULL) {
        keyParts++;
    }
    if (keyParts == 0 || keyParts != (int)foreign->partCount) {
        return 0;
    }
    // The tint half (#830): an untinted row never matches a tinted model, and a
    // tinted row only the one tint it names.
    if ((row->keyTinted != 0) != (foreign->grayscale != 0) ||
        (row->keyTinted != 0 && memcmp(row->tintRgb, foreign->grayscaleRgb, sizeof(row->tintRgb)) != 0)) {
        return 0;
    }
    // The env-colour half (#1167): likewise for a model whose XLU layer sets its
    // colours (MM's songs: one note list, one env colour per song).
    if ((row->keyXluEnv != 0) != (foreign->xluColor.set != 0) ||
        (row->keyXluEnv != 0 && memcmp(row->envRgb, foreign->xluColor.env, sizeof(row->envRgb)) != 0)) {
        return 0;
    }
    for (int i = 0; i < keyParts; i++) {
        const char* dl = foreign->parts[i].dl;
        if (dl == NULL || strncmp(dl, kPartPrefix, sizeof(kPartPrefix) - 1) != 0 ||
            strcmp(dl + sizeof(kPartPrefix) - 1, row->foreignParts[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

int Combo_HostNativeFind(const ComboHostNativeRow* rows, int count, const ComboModel* foreign) {
    if (rows == NULL || foreign == NULL || foreign->partCount == 0 || foreign->partCount > COMBO_MODEL_MAX_PARTS) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (HostNativeRowMatches(&rows[i], foreign)) {
            return i;
        }
    }
    return -1;
}

// ---- Classification ---------------------------------------------------------

static void AnswerNone(ComboModelAnswer* out) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
}

uint8_t Combo_ClassifyForeignModel(uint8_t hostGame, const ComboModel* model, ComboModelAnswer* out) {
    AnswerNone(out);
    if (!ModelSlotValid(hostGame) || !Combo_ModelIsWellFormed(model)) {
        return (uint8_t)COMBO_MODEL_ANSWER_NONE;
    }
    int collides = 0;
    for (uint8_t i = 0; i < model->partCount; i++) {
        collides |= Combo_ForeignModel_PathCollides(model->parts[i].dl);
    }
    if (!collides) {
        if (out != NULL) {
            out->kind = (uint8_t)COMBO_MODEL_ANSWER_DESCRIPTOR;
            out->model = *model;
        }
        return (uint8_t)COMBO_MODEL_ANSWER_DESCRIPTOR;
    }
    // The origin's paths would resolve into the host's own archive: only the
    // host can say what it draws instead (host-native mapping, M7's data).
    ComboHostNativeModelFn mapper = sHostNativeModels[hostGame];
    uint16_t hostKey = 0;
    if (mapper == NULL || mapper(model, &hostKey) != 1) {
        return (uint8_t)COMBO_MODEL_ANSWER_NONE;
    }
    if (out != NULL) {
        out->kind = (uint8_t)COMBO_MODEL_ANSWER_HOST_NATIVE;
        out->hostKey = hostKey;
        out->model = *model;
    }
    return (uint8_t)COMBO_MODEL_ANSWER_HOST_NATIVE;
}

uint8_t Combo_GetForeignItemModel(uint8_t hostGame, SharedItem item, ComboModelAnswer* out) {
    AnswerNone(out);
    if (!ModelSlotValid(hostGame) || !ModelSlotValid(item.originGame) || item.originGame == hostGame) {
        return (uint8_t)COMBO_MODEL_ANSWER_NONE;
    }
    ComboModelSourceFn source = sModelSources[item.originGame];
    ComboModel model;
    Combo_ModelInit(&model);
    if (source == NULL || source(item.id, &model) != 1) {
        return (uint8_t)COMBO_MODEL_ANSWER_NONE;
    }
    return Combo_ClassifyForeignModel(hostGame, &model, out);
}
