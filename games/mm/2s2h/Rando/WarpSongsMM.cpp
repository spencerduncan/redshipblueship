/**
 * @file WarpSongsMM.cpp
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane XW5;
 *        OoTMM songMinuetMm, songBoleroMm, songSerenadeMm, songRequiemMm,
 *        songNocturneMm and songPreludeMm). The MM direction of XW4's Song of
 *        Soaring (OoT), on the item pipeline masks P5 built (OoTMasksMM.cpp).
 *
 * THE ITEMS. Under its frozen second-word rule (RSBS_XITEM2_SONG_MINUET_MM ...,
 * hidden until lane XW12) MM's pool gains one copy of that song
 * (Logic/GeneratePools.cpp, MM_WarpSongMM_PooledCopies), ids APPENDED before
 * RI_MAX_TRAP after the masks (Types.h). Each is named as OoTMM names it
 * ("Minuet of Forest (MM)", StaticData/Items.cpp), a RITYPE_MAJOR row, so MM's
 * fill and the bag treat it as progression. Its give sets an OWNED BIT
 * (GiveItem.cpp), and a second copy converts to junk (ConvertItem.cpp). Under the
 * shared rule the MM copy is held back (warp_songs_mm.h): the shared family is
 * lane XW10's.
 *
 * WHERE OWNERSHIP IS STORED. In gComboCtx.mmWarpSongsOwned (context.h, ADR 0009
 * claim 20), one bit per song, not in MM's half: RandoSaveInfo has no padding and
 * randoInf two free bits, so six flags there would shift every later SaveContext
 * field of every older .redsave; the slot state byte P5 used has one free bit. A
 * starting song is the WORLD's (a seed package's world record carries the byte),
 * a pickup is the session's (every commit carries it), and every bracket that
 * restores MM's save after a SIMULATED playthrough restores this byte too
 * (MM_WarpSongsMM_SnapshotOwned / _RestoreOwned): the bag's MM engine, MM's
 * crawl (Logic.cpp), its glitchless fill and the attempt ladder (OnFileCreate.cpp)
 * all give placed items into the live save, and a song given there must not
 * become the player's.
 *
 * WHAT OWNING ONE DOES. Nothing in play yet: lane XW7 adds MM's note matcher, the
 * warp prompt, the departure through En_Test7 and the crossing. MM's logic knows
 * only how to PLAY each one (Logic.h, CAN_PLAY_OOT_WARP_SONG: the ocarina, the
 * song under its rule, and OoTMM's can_play_tp_* buttons, which Ocarina Button
 * Shuffle decides), and no region uses that until lane XW9's travel facts. Neither
 * game's pause screen has a row for a foreign song; lane XW11 adds them.
 *
 * ITS MODEL (a #1185 descriptor, the shape P5 used for the masks): Ocarina of
 * Time's own song asked through the co-mounted archives
 * (MM_ForeignModel_DrawForeignItem), which MM's host-native table draws as MM's
 * own note in that song's colour (ForeignModelSingleExe.cpp's kNoteMinuet,
 * kNoteBolero and the GID_04 .. GID_07 rows); MM's generic white note when nothing
 * answers. Hosted by OoT (a crossing), the item answers "a 2S2H custom draw that
 * is not re-expressed" (IsCustomDrawn), the masks' answer.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstring>

#include "Rando/Rando.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "Rando/WarpSongsMM.h"

#include "context.h"       // src/common: SharedItem, GAME_OOT
#include "foreign_items.h" // src/common: the second rules word, Combo_GetForeignItemByNameFor
#include "warp_songs_mm.h" // src/common: the copies rule, the names, the ownership byte

extern "C" {
#include "variables.h"
#include "functions.h"
#include "objects/object_gi_melody/object_gi_melody.h" // gGiSongNoteDL
// ForeignModelSingleExe.cpp (#980): the other game's item model, MM as the host.
int MM_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
}

// THE APPEND-ONLY LOCKS. The six ids sit right after the masks P5 appended, so no
// id a world, a save or a crossing already holds moves.
static_assert(RI_SONG_MINUET == RI_MASK_SPOOKY + 1 && RI_SONG_PRELUDE == RI_SONG_MINUET + 5,
              "RI_SONG_MINUET .. RI_SONG_PRELUDE are appended right after the masks P5 added (#1169 XW5)");

namespace {

struct WarpSong {
    RandoItemId ri;
    uint32_t ruleBit;
};

// OoTMM's order, which is the order of the rule bits and the ownership bits.
constexpr WarpSong kWarpSongs[] = {
    { RI_SONG_MINUET, RSBS_XITEM2_SONG_MINUET_MM },     { RI_SONG_BOLERO, RSBS_XITEM2_SONG_BOLERO_MM },
    { RI_SONG_SERENADE, RSBS_XITEM2_SONG_SERENADE_MM }, { RI_SONG_REQUIEM, RSBS_XITEM2_SONG_REQUIEM_MM },
    { RI_SONG_NOCTURNE, RSBS_XITEM2_SONG_NOCTURNE_MM }, { RI_SONG_PRELUDE, RSBS_XITEM2_SONG_PRELUDE_MM },
};
static_assert(sizeof(kWarpSongs) / sizeof(kWarpSongs[0]) == RSBS_MM_WARP_SONG_COUNT, "one row per MM warp song");

/** The song's index in kWarpSongs, or -1. */
int IndexOf(uint16_t riId) {
    for (int i = 0; i < (int)RSBS_MM_WARP_SONG_COUNT; i++) {
        if ((uint16_t)kWarpSongs[i].ri == riId) {
            return i;
        }
    }
    return -1;
}

/** MM's generic note: DrawSong's draw (DrawItem.cpp) in white. */
void DrawGenericNote() {
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    MM_gSPDisplayList(POLY_XLU_DISP++, (Gfx*)&gGiSongNoteDL);
    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
}

} // namespace

extern "C" uint32_t MM_WarpSongMM_RuleBit(uint16_t riId) {
    const int i = IndexOf(riId);
    return i >= 0 ? kWarpSongs[i].ruleBit : 0u;
}

extern "C" uint16_t MM_WarpSongMM_ItemId(uint32_t ruleBit) {
    for (const WarpSong& song : kWarpSongs) {
        if (song.ruleBit == ruleBit) {
            return (uint16_t)song.ri;
        }
    }
    return (uint16_t)RI_UNKNOWN;
}

extern "C" bool MM_WarpSongMM_RuleOn(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    return bit != 0u && Combo_ComboCrossItemRule2(bit);
}

extern "C" int MM_WarpSongMM_PooledCopies(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    return bit != 0u ? (int)Combo_MMWarpSongCopies(Combo_ComboCrossItemFlags2(), bit) : 0;
}

extern "C" bool MM_WarpSongMM_Owned(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    return bit != 0u && Combo_MMWarpSongOwned(bit);
}

extern "C" void MM_WarpSongMM_Give(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    if (bit != 0u) {
        Combo_MMWarpSongGive(bit);
    }
}

extern "C" bool MM_WarpSongMM_Usable(uint16_t riId) {
    return MM_WarpSongMM_RuleOn(riId) && MM_WarpSongMM_Owned(riId);
}

extern "C" uint8_t MM_WarpSongsMM_SnapshotOwned(void) {
    return Combo_MMWarpSongsOwned();
}

extern "C" void MM_WarpSongsMM_RestoreOwned(uint8_t owned) {
    Combo_MMWarpSongsSetOwned(owned);
}

extern "C" const char* MM_WarpSongMM_IconPath(uint16_t riId) {
    if (IndexOf(riId) < 0) {
        return nullptr;
    }
    // MM's own song note: every song of MM's quest page draws this texture.
    return (const char*)MM_gItemIcons[ITEM_SONG_SOARING];
}

extern "C" void MM_WarpSongMM_DrawGetItem(uint16_t riId) {
    const int i = IndexOf(riId);
    if (i < 0 || MM_gPlayState == nullptr) {
        return;
    }
    // Ocarina of Time's own song, by the name OoT's item table gives it, through
    // the co-mounted archives. Looked up once per song, a miss included (-1 not
    // yet asked, 0 no such OoT item, 1 found; StoneMaskOoT.cpp's shape): the name
    // registry is static once both games' tables are registered at bring-up, and
    // a miss asked again every frame would walk the whole registry per draw.
    static SharedItem sOoTSong[RSBS_MM_WARP_SONG_COUNT];
    static_assert(RSBS_MM_WARP_SONG_COUNT == 6u, "one 'not yet asked' (-1) per MM warp song below");
    static int8_t sResolved[RSBS_MM_WARP_SONG_COUNT] = { -1, -1, -1, -1, -1, -1 };
    if (sResolved[i] < 0) {
        const char* name = Combo_MMWarpSongOoTName(kWarpSongs[i].ruleBit);
        sResolved[i] = name != nullptr && Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, name, &sOoTSong[i]) ? 1 : 0;
    }
    if (sResolved[i] == 1 &&
        MM_ForeignModel_DrawForeignItem(MM_gPlayState, sOoTSong[i].originGame, sOoTSong[i].id) == 1) {
        return;
    }
    DrawGenericNote();
}

#endif // RSBS_SINGLE_EXECUTABLE
