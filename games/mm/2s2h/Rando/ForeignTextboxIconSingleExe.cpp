/**
 * @file ForeignTextboxIconSingleExe.cpp
 * @brief A foreign (OoT-origin) item's real icon inside MM's blue get-item
 *        textbox (#607, Tier 2b of #494).
 *
 * THE PROBLEM. MM's textbox names its icon with one header byte, a GetItemId
 * (CustomMessage::Entry::icon, buff[2]). z_message.c's Message_DecodeHeader maps
 * that byte through D_801CFF94 to an MM ItemId, MM_Message_LoadItemIcon points
 * msgCtx->textboxSegment[TEXTBOX_SEG_ICON] at MM's own texture for that id, and
 * MM_Message_DrawItemIcon draws the segment with a layout chosen by the id's
 * range. An OoT item has no GetItemId in MM, so no header byte names it; since
 * #510 the foreign pickup has used 0xFE, "no icon".
 *
 * THE PATH (smallest surgery; one fenced vendored line):
 *
 *   1. CheckQueue's foreign branch asks ForeignTextboxIconForCheck (below): the
 *      ORIGIN game answers the texture path and its layout
 *      (Combo_GetForeignItemTextboxIcon, src/common/foreign_textbox_icon.h), and
 *      this TU picks the MM item id whose EXISTING textbox branch draws that
 *      layout (MM_ForeignTextboxIcon_ItemIdForShape). Both ride the Entry
 *      (foreignIconTexture / foreignIconItemId); the header byte stays 0xFE.
 *   2. CustomMessage::LoadCustomMessageIntoFont arms this TU with the Entry's
 *      pair on EVERY custom load (null for every other message, which disarms).
 *   3. The fenced hook at the end of Message_DecodeHeader consumes the arm and,
 *      for the custom message it was armed for, does what the vendored icon
 *      branch would have done for a native icon (itemId, unk11F18,
 *      MM_Message_LoadItemIcon for the geometry) and then points the icon
 *      segment at the OoT texture. MM_Message_DrawItemIcon is untouched: the
 *      chosen item id routes the draw through the branch that loads the right
 *      format and size.
 *
 * WHY NOT A NEW HEADER BYTE VALUE. The byte is decoded before any hook sees the
 * message and every value that reaches LoadItemIcon already names an MM icon;
 * a sentinel would be one more vanilla table row to keep out of D_801CFF94.
 * Keeping 0xFE also makes the fallback free: when anything in the chain says
 * no (no placement, no origin answer, the origin's archive not mounted, the arm
 * consumed by a different message), the textbox is exactly the pre-#607 one.
 *
 * OoTMM, for the record (the operator's reference for player-facing choices):
 * its MM get-item text header is 0xFE for EVERY item, native or foreign
 * (packages/generator/src/common/text/text.c comboTextAppendHeader), because it
 * identifies the item by drawing its real 3D model. We cannot draw OoT's model
 * in MM (docs/resource-namespace-audit.md; the foreign pickup draws no model
 * since #510), and 2S2H's own rando textbox carries the icon for every native
 * pickup, so the icon is the identification surface that exists here; with it
 * a foreign pickup reads like a native one, which is #510's rule.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "ForeignTextboxIcon.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/CustomMessage/CustomMessage.h"

extern "C" {
#include "variables.h" // MM_gPlayState
#include "functions.h"
extern s16 D_801CFF94[250];
// MM's song-note tints, indexed by ItemId - ITEM_SONG_SONATA (z_message.c).
extern s16 D_801CFE04[];
extern s16 D_801CFE1C[];
extern s16 D_801CFE34[];
}

// src/common. Outside any extern "C" block: each header manages its own linkage.
#include "foreign_items.h"
#include "foreign_textbox_icon.h"

namespace {

constexpr uint8_t kNoIcon = 0xFE;

// The arm: what the most recent custom message load asked for.
const char* sArmedTexture = nullptr;
uint8_t sArmedItemId = kNoIcon;

// TEST ONLY: -1 = ask the archive manager; 0 / 1 = answer that.
int sMountOverride = -1;

bool TextureMounted(const char* texture) {
    if (sMountOverride >= 0) {
        return sMountOverride == 1;
    }
    static const char kOtr[] = "__OTR__";
    if (texture == nullptr || std::strncmp(texture, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    // Without the archive the draw would load a path nothing can resolve. oot.o2r
    // and soh.o2r are mounted from OoT's first boot on (rsbs/src/main.cpp,
    // Combo_EnsureGameArchivesLoaded), so this answers no only on an MM-first
    // session that never entered OoT — which then shows the pre-#607 textbox.
    return archives != nullptr && archives->HasFile(std::string(texture + sizeof(kOtr) - 1));
}

} // namespace

/**
 * The MM item id whose Message_LoadItemIcon / MM_Message_DrawItemIcon branch
 * draws `icon`'s layout from the icon segment, or 0xFE for none:
 *
 *   ITEM  (RGBA32 32x32) -> ITEM_BOW: the `itemId <= ITEM_REMAINS_TWINMOLD` load
 *                           branch (32-wide box) and the draw's final RGBA32 32x32.
 *   QUEST (RGBA32 24x24) -> ITEM_SKULL_TOKEN: the `>= ITEM_SKULL_TOKEN` load and
 *                           draw branches, RGBA32 24x24.
 *   NOTE  (IA8 16x24)    -> the MM song whose note tint IS the origin's tint (MM's
 *                           table is OoT's vanilla palette: Sonata = Minuet green,
 *                           Goron Lullaby = Bolero red, ...), else the white Song
 *                           of Time; the song branch draws IA8 16x24 tinted.
 *   RUPEE (IA8 16x16)    -> ITEM_RUPEE_GREEN: MM draws its own counter icon there,
 *                           which is the same icon.
 */
extern "C" uint8_t MM_ForeignTextboxIcon_ItemIdForShape(const ComboTextboxIcon* icon) {
    if (icon == nullptr) {
        return kNoIcon;
    }
    switch (icon->shape) {
        case COMBO_TEXTBOX_ICON_ITEM:
            return ITEM_BOW;
        case COMBO_TEXTBOX_ICON_QUEST:
            return ITEM_SKULL_TOKEN;
        case COMBO_TEXTBOX_ICON_RUPEE:
            return ITEM_RUPEE_GREEN;
        case COMBO_TEXTBOX_ICON_NOTE:
            for (int song = ITEM_SONG_SONATA; song <= ITEM_SONG_SUN; song++) {
                const int i = song - ITEM_SONG_SONATA;
                if (D_801CFE04[i] == icon->r && D_801CFE1C[i] == icon->g && D_801CFE34[i] == icon->b) {
                    return (uint8_t)song;
                }
            }
            return ITEM_SONG_TIME;
        default:
            return kNoIcon;
    }
}

namespace Rando {
namespace Foreign {

bool ForeignTextboxIconForCheck(RandoCheckId randoCheckId, const char** texture, uint8_t* textboxItemId) {
    *texture = nullptr;
    *textboxItemId = kNoIcon;
    if (randoCheckId == RC_UNKNOWN) {
        return false;
    }
    const SharedItem* item = Combo_GetForeignPlacementForCheck((uint16_t)randoCheckId);
    ComboTextboxIcon icon;
    if (item == nullptr || Combo_GetForeignItemTextboxIcon(*item, &icon) != 1 || !TextureMounted(icon.texture)) {
        return false;
    }
    const uint8_t itemId = MM_ForeignTextboxIcon_ItemIdForShape(&icon);
    if (itemId == kNoIcon) {
        return false;
    }
    *texture = icon.texture;
    *textboxItemId = itemId;
    return true;
}

} // namespace Foreign
} // namespace Rando

extern "C" void MM_ForeignTextboxIcon_Arm(const char* texture, uint8_t textboxItemId) {
    const bool armed = texture != nullptr && texture[0] != '\0' && textboxItemId != kNoIcon;
    sArmedTexture = armed ? texture : nullptr;
    sArmedItemId = armed ? textboxItemId : kNoIcon;
}

extern "C" void MM_ForeignTextboxIcon_OnDecodeHeader(PlayState* play) {
    const char* texture = sArmedTexture;
    const uint8_t itemId = sArmedItemId;
    // Consumed by the first header decode after the load, whichever message it
    // is: the load that armed it and the decode of the same message are one
    // pair (OnOpenText loads, the next decode reads that buffer).
    MM_ForeignTextboxIcon_Arm(nullptr, kNoIcon);
    if (texture == nullptr || play == nullptr) {
        return;
    }
    MessageContext* msgCtx = &play->msgCtx;
    // Belt and braces: only the custom message. A load whose decode never ran
    // (a textbox closed the frame it opened) cannot dress up the next VANILLA
    // message, which does not pass through LoadCustomMessageIntoFont to disarm.
    if (msgCtx->currentTextId != CUSTOM_MESSAGE_ID) {
        return;
    }
    // What Message_DecodeHeader's icon branch does for a native icon byte, then
    // the one difference: the segment names the origin game's texture.
    msgCtx->unk11F18 = 0;
    msgCtx->itemId = itemId;
    MM_Message_LoadItemIcon(play, itemId, msgCtx->textboxY + 10);
    msgCtx->textboxSegment[TEXTBOX_SEG_ICON] = (char*)texture;
}

// ============================================================================
// TEST BRIDGES (redship tier; src/common/tests/test_foreign_textbox_icon.c)
// ============================================================================

extern "C" void MM_ForeignTextboxIcon_TestSetMountOverride(int value) {
    sMountOverride = value;
}

extern "C" int MM_ForeignTextboxIcon_TestForCheck(uint16_t mmCheckId, const char** texture, uint8_t* textboxItemId) {
    return Rando::Foreign::ForeignTextboxIconForCheck((RandoCheckId)mmCheckId, texture, textboxItemId) ? 1 : 0;
}

/** 1 when `icon` is a NOTE whose chosen MM song draws with exactly the origin's
 *  tint (a white note lands on a white song), or when it is not a NOTE. */
extern "C" int MM_ForeignTextboxIcon_TestNoteTintMatches(const ComboTextboxIcon* icon) {
    if (icon == nullptr || icon->shape != COMBO_TEXTBOX_ICON_NOTE) {
        return 1;
    }
    const int song = MM_ForeignTextboxIcon_ItemIdForShape(icon);
    if (song < ITEM_SONG_SONATA || song > ITEM_SONG_SUN) {
        return 0;
    }
    const int i = song - ITEM_SONG_SONATA;
    return (D_801CFE04[i] == icon->r && D_801CFE1C[i] == icon->g && D_801CFE34[i] == icon->b) ? 1 : 0;
}

/** Any real MM check id (the placement table keys on it). */
extern "C" uint16_t MM_ForeignTextboxIcon_TestSomeCheck(void) {
    return (uint16_t)RC_CLOCK_TOWN_BOMBERS_NOTEBOOK;
}

namespace {

#define FTI_EXPECT(cond, ...)                                         \
    do {                                                              \
        if (!(cond)) {                                                \
            std::printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
            return false;                                             \
        }                                                             \
    } while (0)

struct FakePlay {
    PlayState* play = nullptr;
    char* segments[8] = {};
    PlayState* savedGlobal = nullptr;

    FakePlay() {
        play = (PlayState*)std::calloc(1, sizeof(PlayState));
        play->msgCtx.textboxSegment = segments;
        savedGlobal = MM_gPlayState;
        MM_gPlayState = play;
    }
    ~FakePlay() {
        MM_gPlayState = savedGlobal;
        std::free(play);
        MM_ForeignTextboxIcon_Arm(nullptr, kNoIcon);
    }
};

/** Load `entry` exactly as the OnOpenText hook does, open it as `textId`, and run
 *  the REAL vendored header decode (the fenced hook included). */
void LoadAndDecode(FakePlay& fake, const CustomMessage::Entry& entry, u16 textId) {
    MessageContext* msgCtx = &fake.play->msgCtx;
    CustomMessage::LoadCustomMessageIntoFont(entry);
    msgCtx->currentTextId = textId;
    msgCtx->msgBufPos = 0;
    msgCtx->itemId = kNoIcon;
    msgCtx->unk12014 = 0;
    fake.segments[TEXTBOX_SEG_ICON] = nullptr;
    Message_DecodeHeader(fake.play);
}

CustomMessage::Entry ForeignEntry(const char* texture, uint8_t itemId) {
    CustomMessage::Entry entry;
    entry.textboxType = 2;
    entry.icon = kNoIcon;
    entry.msg = "You found the Hookshot!";
    entry.foreignIconTexture = texture;
    entry.foreignIconItemId = itemId;
    return entry;
}

/** The render tile's format/size and dimensions MM_Message_DrawItemIcon loads,
 *  read back from the display list it writes. */
struct DrawnTile {
    int fmt = -1;
    int siz = -1;
    int width = 0;
    int height = 0;
};

DrawnTile DrawAndReadTile(PlayState* play) {
    static Gfx sGfx[256];
    std::memset(sGfx, 0, sizeof(sGfx));
    Gfx* gfx = sGfx;
    MM_Message_DrawItemIcon(play, &gfx);
    DrawnTile tile;
    for (Gfx* g = sGfx; g < gfx; g++) {
        const uint32_t w0 = (uint32_t)g->words.w0;
        const uint32_t w1 = (uint32_t)g->words.w1;
        const uint32_t op = (w0 >> 24) & 0xFF;
        const uint32_t tileIdx = (w1 >> 24) & 0x7;
        if (op == (uint32_t)(uint8_t)G_SETTILE && tileIdx == G_TX_RENDERTILE) {
            tile.fmt = (int)((w0 >> 21) & 0x7);
            tile.siz = (int)((w0 >> 19) & 0x3);
        } else if (op == (uint32_t)(uint8_t)G_SETTILESIZE && tileIdx == G_TX_RENDERTILE) {
            tile.width = (int)(((w1 >> 12) & 0xFFF) >> G_TEXTURE_IMAGE_FRAC) + 1;
            tile.height = (int)((w1 & 0xFFF) >> G_TEXTURE_IMAGE_FRAC) + 1;
        }
    }
    return tile;
}

struct ShapeCase {
    ComboTextboxIcon icon;
    const char* label;
    int fmt;
    int siz;
    int width;
    int height;
    bool drawsSegment; // false: MM's rupee branch draws its own counter texture
};

bool RunDecodeChain() {
    static const char kFakeOoTTex[] = "__OTR__textures/icon_item_static/gItemIconHookshotTex";

    // D1: a foreign entry reaches the textbox with an icon, and the icon segment
    // names the OoT texture, through the real load and the real decode.
    {
        FakePlay fake;
        LoadAndDecode(fake, ForeignEntry(kFakeOoTTex, ITEM_BOW), CUSTOM_MESSAGE_ID);
        MessageContext* msgCtx = &fake.play->msgCtx;
        FTI_EXPECT(msgCtx->itemId == ITEM_BOW, "D1 itemId %d, want ITEM_BOW (the fenced hook did not run?)",
                   (int)msgCtx->itemId);
        FTI_EXPECT(fake.segments[TEXTBOX_SEG_ICON] == kFakeOoTTex, "D1 icon segment is not the OoT texture");
        FTI_EXPECT(msgCtx->unk12014 == 0x20, "D1 icon box width %d, want 0x20", (int)msgCtx->unk12014);

        // D2: consumed. The next custom message with no foreign icon decodes to
        // the icon-less textbox, byte for byte the pre-#607 foreign pickup.
        LoadAndDecode(fake, ForeignEntry(nullptr, kNoIcon), CUSTOM_MESSAGE_ID);
        FTI_EXPECT(msgCtx->itemId == kNoIcon, "D2 itemId %d after an icon-less load, want 0xFE", (int)msgCtx->itemId);
        FTI_EXPECT(fake.segments[TEXTBOX_SEG_ICON] == nullptr, "D2 icon segment touched");

        // D3: an arm is consumed even by a decode it does not apply to — a load
        // whose textbox never decoded cannot dress up the next (vanilla) message.
        LoadAndDecode(fake, ForeignEntry(kFakeOoTTex, ITEM_BOW), 0x1234);
        FTI_EXPECT(msgCtx->itemId == kNoIcon, "D3 the arm applied to text 0x1234");
        // Re-decode the same buffer as the custom id with no load in between: had
        // the refused decode left the arm standing, this is where it would land.
        msgCtx->currentTextId = CUSTOM_MESSAGE_ID;
        msgCtx->msgBufPos = 0;
        Message_DecodeHeader(fake.play);
        FTI_EXPECT(msgCtx->itemId == kNoIcon, "D3 the arm survived a refused decode");

        // D4: native icons are untouched: an Entry with an MM header icon byte and
        // no foreign texture decodes exactly as vanilla.
        CustomMessage::Entry native = ForeignEntry(nullptr, kNoIcon);
        native.icon = GI_HOOKSHOT;
        LoadAndDecode(fake, native, CUSTOM_MESSAGE_ID);
        FTI_EXPECT(msgCtx->itemId == (u16)D_801CFF94[GI_HOOKSHOT], "D4 native itemId %d, want %d", (int)msgCtx->itemId,
                   (int)D_801CFF94[GI_HOOKSHOT]);
        FTI_EXPECT(fake.segments[TEXTBOX_SEG_ICON] != nullptr && fake.segments[TEXTBOX_SEG_ICON] != kFakeOoTTex,
                   "D4 native icon segment was replaced");
    }

    // D5: every shape lands in a draw branch that reads the icon at exactly the
    // origin texture's layout (the real MM_Message_DrawItemIcon, display list
    // read back), and the song notes find MM's matching tint.
    const ShapeCase cases[] = {
        { { kFakeOoTTex, COMBO_TEXTBOX_ICON_ITEM, 255, 255, 255 }, "item", G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, true },
        { { "__OTR__textures/icon_item_24_static/gQuestIconSmallKeyTex", COMBO_TEXTBOX_ICON_QUEST, 255, 255, 255 },
          "quest",
          G_IM_FMT_RGBA,
          G_IM_SIZ_32b,
          24,
          24,
          true },
        { { "__OTR__textures/icon_item_static/gSongNoteTex", COMBO_TEXTBOX_ICON_NOTE, 150, 255, 100 },
          "note(minuet)",
          G_IM_FMT_IA,
          G_IM_SIZ_8b,
          16,
          24,
          true },
        { { "__OTR__textures/icon_item_static/gSongNoteTex", COMBO_TEXTBOX_ICON_NOTE, 255, 255, 255 },
          "note(white)",
          G_IM_FMT_IA,
          G_IM_SIZ_8b,
          16,
          24,
          true },
        { { "__OTR__textures/parameter_static/gRupeeCounterIconTex", COMBO_TEXTBOX_ICON_RUPEE, 42, 169, 40 },
          "rupee",
          G_IM_FMT_IA,
          G_IM_SIZ_8b,
          16,
          16,
          false },
    };
    for (const ShapeCase& c : cases) {
        const uint8_t itemId = MM_ForeignTextboxIcon_ItemIdForShape(&c.icon);
        FTI_EXPECT(itemId != kNoIcon, "D5 %s has no MM branch", c.label);
        FakePlay fake;
        LoadAndDecode(fake, ForeignEntry(c.icon.texture, itemId), CUSTOM_MESSAGE_ID);
        FTI_EXPECT(fake.play->msgCtx.itemId == itemId, "D5 %s itemId %d, want %d", c.label,
                   (int)fake.play->msgCtx.itemId, (int)itemId);
        FTI_EXPECT(!c.drawsSegment || fake.segments[TEXTBOX_SEG_ICON] == c.icon.texture, "D5 %s segment", c.label);
        const DrawnTile tile = DrawAndReadTile(fake.play);
        FTI_EXPECT(tile.fmt == c.fmt && tile.siz == c.siz && tile.width == c.width && tile.height == c.height,
                   "D5 %s draws fmt %d siz %d %dx%d, want fmt %d siz %d %dx%d", c.label, tile.fmt, tile.siz, tile.width,
                   tile.height, c.fmt, c.siz, c.width, c.height);
        if (c.icon.shape == COMBO_TEXTBOX_ICON_NOTE) {
            const int i = itemId - ITEM_SONG_SONATA;
            FTI_EXPECT(i >= 0 && i <= ITEM_SONG_SUN - ITEM_SONG_SONATA, "D5 %s is not a song", c.label);
            FTI_EXPECT(D_801CFE04[i] == c.icon.r && D_801CFE1C[i] == c.icon.g && D_801CFE34[i] == c.icon.b,
                       "D5 %s tint (%d,%d,%d) is not MM's (%d,%d,%d)", c.label, c.icon.r, c.icon.g, c.icon.b,
                       D_801CFE04[i], D_801CFE1C[i], D_801CFE34[i]);
        }
    }
    const ComboTextboxIcon none = { kFakeOoTTex, COMBO_TEXTBOX_ICON_NONE, 255, 255, 255 };
    FTI_EXPECT(MM_ForeignTextboxIcon_ItemIdForShape(&none) == kNoIcon, "D5 shape NONE has a branch");
    FTI_EXPECT(MM_ForeignTextboxIcon_ItemIdForShape(nullptr) == kNoIcon, "D5 null icon has a branch");
    return true;
}

#undef FTI_EXPECT

} // namespace

/** The MM half of the ForeignTextboxIcon row: 0 on success. */
extern "C" int MM_ForeignTextboxIcon_RunHeadless(void) {
    return RunDecodeChain() ? 0 : 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
