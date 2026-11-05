/**
 * @file test_song_soaring_oot.c
 * @brief Locks for the Song of Soaring (OoT) item and OoTMM's "Not enough
 *        locations" refusal (#1169 lane XW4; OoTMM songSoaringOot and
 *        sharedSongSoaring; the item is
 *        games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp, the count and
 *        the wording src/common/warp_song_items.c).
 *
 * Five rows, split by what each one needs:
 *
 *   oot-soaring-table-bounds (CTest OoTSoaringTableBounds, redship tier,
 *   ROM-free) -- THE TABLES. RG_SONG_OF_SOARING_OOT, its RHT key and its RAND_INF
 *   flag are each appended right after the Stone Mask's (so no id a world or a
 *   save holds moves), the RAND_INF flags still fit OoT's 129 saved words, no
 *   ItemID was added (OoT_gItemIcons still ends at ITEM_MASK_STONE, so none of the
 *   vanilla-sized ItemID tables is reached), and the item table's row: a song
 *   (the restricted song passes take it), an advancement item, a MOD_RANDOMIZER
 *   row whose ids are its RG id (256 or above, never an ItemID), the melody
 *   object and OoT's generic note row inside the draw table, its own custom draw,
 *   the custom-item text, a MAJOR item, its hint key, Logic's flag mapping and
 *   OoTMM's name. Its textbox icon in Majora's Mask is OoT's white note. From
 *   source: its trick names and its give's flag mapping exist.
 *
 *   oot-soaring-rule-logic (CTest OoTSoaringRuleLogic, redship tier, ROM-free)
 *   -- THE RULE. The pooled-copies rule over every combination of the two bits;
 *   OoT's REAL logic: CanUse only for a child holding the song with an ocarina
 *   and the C-down, C-up and C-left buttons, only while the rule is frozen on
 *   (A and C-right never matter); the logic's item effect sets the flag and no
 *   quest bit (the song's quest-table row would be the Forest Medallion's 0);
 *   OoTMM's refusal wording and arithmetic; the option text's warning.
 *
 *   oot-soaring-rules-off-unchanged (CTest OoTSoaringRulesOffUnchanged, redship
 *   tier, ROM-free) -- INERT. Both Soaring bits are refused at Generate while
 *   hidden (the seam off), and from source that gate precedes the fill; a
 *   rules-off world pools no copy and never credits the song; the refusal never
 *   fires for twelve or fewer songs in either restricted mode (today's song
 *   pass); main's ids for the masks before it are pinned; from source, the pool
 *   adds the item only under the frozen rule and the refusal sits between the
 *   #1085 hold-back and the song fill.
 *
 *   oot-soaring-pooled-only-when-rule-on (CTest OoTSoaringPooledOnlyWhenRuleOn,
 *   rando tier) -- THE POOL, over real paired OoT generations. Rule off: no copy
 *   anywhere in OoT's world. Rule on through the TEST SEAM
 *   (Combo_ComboItemRules2RevealForTest; production never calls it): exactly one
 *   copy, under Shuffle Songs Anywhere and Off. Rule and shared rule on: none (the
 *   family path, held back). Then its hint text names it and OoT's REAL give sets
 *   its flag and nothing else.
 *
 *   song-locations-overflow-refused (CTest SongLocationsOverflowRefused, rando
 *   tier) -- THE REFUSAL, over real paired OoT generations through the seam:
 *   thirteen songs on twelve spots is refused with OoTMM's wording under Song
 *   Locations and under Dungeon Rewards, and nothing stays frozen; a starting
 *   song, the Shared Song of Time (#1085's hold-back), the Shared Song of Soaring
 *   and Anywhere each generate; and with the rule off Song Locations generates as
 *   it always has.
 *
 *   Both rando rows are skipped by `--test all`, like every other generation
 *   row (test_runner.cpp's skip list): the world a generation makes stays alive
 *   in the process (Rando::Settings holds its heap Rando::Context), and the
 *   ROM-free rows after them in the table, combo-crossing-views first, assert
 *   that no heap world survives a release.
 *
 * Every combo key is read through Combo_ComboSettingKey, so the tier-4
 * classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Xw4 prefix because every included test file shares one translation unit.
 */

#include "../combo_settings_view.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../foreign_model.h"
#include "../foreign_textbox_icon.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../warp_song_items.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/oot_song_of_soaring_oot_test.cpp
int OoT_SoaringTest_PoolId(void);
int OoT_SoaringTest_Id(int which);
const char* OoT_SoaringTest_Name(void);
int OoT_SoaringTest_RowFact(int fact);
int OoT_SoaringTest_Logic(int held, int child, int ocarina, uint32_t buttons, int which);
int OoT_SoaringTest_ApplyEffect(uint32_t* questItems);
int OoT_SoaringTest_WorldCopies(void);
int OoT_SoaringTest_OnSongLocations(void);
int OoT_SoaringTest_HintNamesSong(void);
int OoT_SoaringTest_RealGive(void);
// games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp
bool OoT_SongOfSoaringOoTOn(void);
int OoT_SongOfSoaringOoTPooledCopies(void);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
int Rando_HeadlessSeedTest(const char* seedStr);
// games/oot/soh/Enhancements/randomizer/ForeignModelOoT.cpp
const char* OoT_ComboModel_TestItemReason(uint16_t id);
#ifdef __cplusplus
}
#endif

static int sXw4Failures;

#define XW4_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sXw4Failures++;                           \
        }                                             \
    } while (0)

namespace {

// The second-word bits, spelled as literals: they are .redsave and seed-package
// format, so the lock states them rather than reading them back from the header.
const uint32_t kXw4Soaring = 0x00000001u;       // songSoaringOot
const uint32_t kXw4SharedSoaring = 0x00000080u; // sharedSongSoaring
const uint32_t kXw4Seed = 0x1169A4A4u;
const uint32_t kXw4SettingsHash = 0x1169CA5Eu;
const uint32_t kXw4ProfileDigest = 0x1169D16Eu;

// The ocarina buttons the logic probe takes: bit 0 A, 1 C-up, 2 C-down, 3 C-left,
// 4 C-right.
const uint32_t kXw4ButtonA = 0x01u;
const uint32_t kXw4ButtonUp = 0x02u;
const uint32_t kXw4ButtonDown = 0x04u;
const uint32_t kXw4ButtonLeft = 0x08u;
const uint32_t kXw4ButtonRight = 0x10u;
const uint32_t kXw4SoaringButtons = kXw4ButtonDown | kXw4ButtonUp | kXw4ButtonLeft;
const uint32_t kXw4AllButtons = 0x1Fu;

/** Publish a paired identity and freeze the shipped combo record, every
 *  first-word rule off and @p rules2 as the second word, the way Playthrough_Init
 *  stamps a world (the freeze is raw: the hidden-bit refusal is Generate's). */
void Xw4FreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kXw4Seed;
    gComboCtx.sharedRandoSettingsHash = kXw4SettingsHash;
    gComboCtx.mmProfileDigest = kXw4ProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** The text of @p rel under the checkout, or "" when it cannot be read. */
std::string Xw4ReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)rel;
    return std::string();
#endif
}

/** How many times @p needle occurs in @p hay. */
int Xw4Count(const std::string& hay, const char* needle) {
    int n = 0;
    const size_t len = strlen(needle);
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + len)) {
        n++;
    }
    return n;
}

bool Xw4StartsWith(const char* s, const char* prefix) {
    return s != NULL && strncmp(s, prefix, strlen(prefix)) == 0;
}

} // namespace

// ============================================================================
// oot-soaring-table-bounds
// ============================================================================

TestResult Test_OoTSoaringTableBounds(void) {
    printf("[TEST] oot-soaring-table-bounds: the Song of Soaring (OoT) is appended after the masks in every OoT "
           "enum, adds no ItemID, fits the 129 saved RAND_INF words, and its item row is a song with its own flag, "
           "note, text and hint (#1169 XW4)\n");
    sXw4Failures = 0;
    XW4_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");
    const int soaring = OoT_SoaringTest_PoolId();

    // ---- (1) Appended, never inserted ------------------------------------------------
    {
        const int stone = OoT_SoaringTest_Id(0);
        const int firstLogicOnly = OoT_SoaringTest_Id(1);
        const int rgMax = OoT_SoaringTest_Id(2);
        // Pinned against the Stone Mask before it and the bound after it, never
        // as the last entry: a later append (Kamaro's Mask, another warp song)
        // goes after the song and must not turn this row red.
        XW4_CHECK(soaring == stone + 1 && soaring < firstLogicOnly && firstLogicOnly < rgMax,
                  "RG_SONG_OF_SOARING_OOT is %d; the Stone Mask is %d, the first logic-only id %d, RG_MAX %d: it must "
                  "sit right after the Stone Mask and before the never-placed logic-only ids",
                  soaring, stone, firstLogicOnly, rgMax);
        const int flag = OoT_SoaringTest_Id(3);
        const int stoneFlag = OoT_SoaringTest_Id(4);
        const int infMax = OoT_SoaringTest_Id(5);
        const int savedWords = OoT_SoaringTest_Id(6);
        XW4_CHECK(flag == stoneFlag + 1 && flag < infMax,
                  "RAND_INF_OBTAINED_SONG_OF_SOARING_OOT is %d (Stone Mask's %d, RAND_INF_MAX %d): flags are saved by "
                  "index, so it must follow the Stone Mask's, appended",
                  flag, stoneFlag, infMax);
        XW4_CHECK(savedWords == 129 && infMax <= savedWords * 16,
                  "%d RAND_INF flags in %d saved words: OoT's save keeps 129 words (2064 bits)", infMax, savedWords);
        const int key = OoT_SoaringTest_Id(7);
        const int stoneKey = OoT_SoaringTest_Id(8);
        const int keyMax = OoT_SoaringTest_Id(9);
        XW4_CHECK(key == stoneKey + 1 && key < keyMax,
                  "RHT_SONG_OF_SOARING_OOT is %d (Stone Mask's %d, RHT_MAX %d): it must follow the Stone Mask's, "
                  "appended",
                  key, stoneKey, keyMax);
    }

    // ---- (2) No ItemID: the vanilla-sized ItemID tables are never reached ------------
    {
        const int icons = OoT_SoaringTest_Id(10);
        const int lastCustom = OoT_SoaringTest_Id(11);
        const int slots = OoT_SoaringTest_Id(12);
        XW4_CHECK(icons == lastCustom + 1,
                  "OoT_gItemIcons has %d rows, not ITEM_MASK_STONE (0x%02X) + 1: an ItemID was added, and every "
                  "vanilla-sized ItemID table (OoT_gItemSlots, gItemAgeReqs, sItemActions) must then be bounded "
                  "for it",
                  icons, (unsigned)lastCustom);
        XW4_CHECK(slots == 56, "OoT_gItemSlots has %d entries, not vanilla's 56", slots);
        XW4_CHECK(OoT_SoaringTest_RowFact(3) == 1,
                  "the song's GetItemEntry carries an itemId that could be an OoT ItemID (it must be its RG id, "
                  "256 or above)");
    }

    // ---- (3) The item table's row ---------------------------------------------------
    {
        static const char* const kFacts[] = {
            "a song (ITEMTYPE_SONG)",
            "an advancement item",
            "a MOD_RANDOMIZER row whose getItemId is its RG id",
            "an entry itemId that is its RG id",
            "drawn from OBJECT_GI_MELODY",
            "OoT's generic note row (GID_SONG_GENERIC) inside the draw table",
            "drawn by Randomizer_DrawSongOfSoaringOoT",
            "the custom-item text",
            "a MAJOR item",
            "hinted as RHT_SONG_OF_SOARING_OOT",
            "owned through RAND_INF_OBTAINED_SONG_OF_SOARING_OOT in Logic's table",
        };
        for (int f = 0; f < (int)(sizeof(kFacts) / sizeof(kFacts[0])); f++) {
            const int got = OoT_SoaringTest_RowFact(f);
            XW4_CHECK(got == 1, "the Song of Soaring (OoT)'s item row is not %s (%d)", kFacts[f], got);
        }
        const char* name = OoT_SoaringTest_Name();
        XW4_CHECK(name != NULL && strcmp(name, "Song of Soaring (OoT)") == 0,
                  "the item is named '%s', not OoTMM's \"Song of Soaring (OoT)\"", name != NULL ? name : "(null)");
    }

    // ---- (4) In Majora's Mask: OoT's white note, as textbox icon and model -----------
    {
        SharedItem item;
        memset(&item, 0, sizeof(item));
        item.originGame = (uint8_t)GAME_OOT;
        item.id = (uint16_t)soaring;
        ComboTextboxIcon icon;
        memset(&icon, 0, sizeof(icon));
        const int answered = Combo_GetForeignItemTextboxIcon(item, &icon);
        const char* tail = "/gSongNoteTex";
        const size_t len = icon.texture != NULL ? strlen(icon.texture) : 0;
        const bool noteTex = len >= strlen(tail) && strcmp(icon.texture + len - strlen(tail), tail) == 0;
        XW4_CHECK(answered == 1 && icon.shape == (uint8_t)COMBO_TEXTBOX_ICON_NOTE && noteTex && icon.r == 255 &&
                      icon.g == 255 && icon.b == 255,
                  "the textbox icon is answered %d, shape %u, '%s' (%u,%u,%u); want OoT's note in white", answered,
                  (unsigned)icon.shape, icon.texture != NULL ? icon.texture : "(null)", icon.r, icon.g, icon.b);

        // Its get-item model there: OoT's source answers its GID_SONG_GENERIC
        // row (the custom draw falls back to it), whose note list both archives
        // carry, so Majora's Mask draws its own white note, not the stand-in.
        const char* why = OoT_ComboModel_TestItemReason((uint16_t)soaring);
        ComboModelAnswer model;
        memset(&model, 0, sizeof(model));
        const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_MM, item, &model);
        XW4_CHECK(why == NULL && kind == (uint8_t)COMBO_MODEL_ANSWER_HOST_NATIVE,
                  "OoT's source answers no model for the song ('%s') or MM does not draw its own note (answer kind %u)",
                  why != NULL ? why : "", (unsigned)kind);
    }

    // ---- (5) From source: the trick names and the give's flag ------------------------
    {
        const std::string shops = Xw4ReadSource("games/oot/soh/Enhancements/randomizer/3drando/shops.cpp");
        const std::string give = Xw4ReadSource("games/oot/soh/Enhancements/randomizer/randomizer.cpp");
        XW4_CHECK(!shops.empty() && !give.empty(), "the checkout's shops.cpp or randomizer.cpp could not be read");
        XW4_CHECK(Xw4Count(shops, "trickNameTable[RG_SONG_OF_SOARING_OOT]") == 1,
                  "shops.cpp has no trick names for the song: an ice trap shown as it would assert");
        XW4_CHECK(Xw4Count(give, "{ RG_SONG_OF_SOARING_OOT, RAND_INF_OBTAINED_SONG_OF_SOARING_OOT }") == 1,
                  "randomizer.cpp's give does not map the song to its flag");
    }

    printf("[TEST] oot-soaring-table-bounds: %s\n", sXw4Failures == 0 ? "PASS" : "FAIL");
    return sXw4Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// oot-soaring-rule-logic
// ============================================================================

TestResult Test_OoTSoaringRuleLogic(void) {
    printf("[TEST] oot-soaring-rule-logic: the frozen rule pools one Song of Soaring (OoT) unless shared, OoT's "
           "logic plays it as a child with the C-down, C-up and C-left buttons only under the rule, its effect is its "
           "flag, and OoTMM's refusal and warning are worded (#1169 XW4)\n");
    sXw4Failures = 0;
    XW4_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");

    // ---- (1) The pooled copies over every combination of the two bits ----------------
    {
        XW4_CHECK(Combo_SongSoaringOoTCopies(0u) == 0u, "no rule pooled a copy");
        XW4_CHECK(Combo_SongSoaringOoTCopies(kXw4Soaring) == 1u, "songSoaringOot alone did not pool one copy");
        XW4_CHECK(Combo_SongSoaringOoTCopies(kXw4Soaring | kXw4SharedSoaring) == 0u,
                  "with sharedSongSoaring the separate copy was pooled: the shared song is the family (XW10), held "
                  "back, never placed as a separate item");
        XW4_CHECK(Combo_SongSoaringOoTCopies(kXw4SharedSoaring) == 0u, "the shared bit alone pooled a copy");
        XW4_CHECK(Combo_SongSoaringOoTCopies(0x3FFFu & ~kXw4SharedSoaring) == 1u,
                  "the other warp-song bits changed the Soaring count");
        ComboContext_Init();
        XW4_CHECK(OoT_SongOfSoaringOoTPooledCopies() == 0 && !OoT_SongOfSoaringOoTOn(),
                  "with nothing frozen OoT pools the song or credits the rule");
        Xw4FreezeWorld(kXw4Soaring);
        XW4_CHECK(OoT_SongOfSoaringOoTPooledCopies() == 1 && OoT_SongOfSoaringOoTOn(),
                  "a world frozen with songSoaringOot pools %d copies (rule read %d)",
                  OoT_SongOfSoaringOoTPooledCopies(), OoT_SongOfSoaringOoTOn() ? 1 : 0);
        Xw4FreezeWorld(kXw4Soaring | kXw4SharedSoaring);
        XW4_CHECK(OoT_SongOfSoaringOoTPooledCopies() == 0,
                  "a world frozen with the shared Soaring pools a separate OoT copy");
    }

    // ---- (2) OoT's REAL logic --------------------------------------------------------
    {
        Xw4FreezeWorld(kXw4Soaring);
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4SoaringButtons, 0) == 1,
                  "rule on: a child with the song, an ocarina and C-down, C-up and C-left cannot play it");
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4AllButtons, 0) == 1,
                  "rule on: a child with every button cannot play it");
        XW4_CHECK(OoT_SoaringTest_Logic(1, 0, 1, kXw4AllButtons, 0) == 0,
                  "rule on: an adult plays it (OoTMM's can_play_soaring is child only; no ageless Soaring here)");
        XW4_CHECK(OoT_SoaringTest_Logic(0, 1, 1, kXw4AllButtons, 0) == 0, "rule on: a child plays it without it");
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 0, kXw4AllButtons, 0) == 0,
                  "rule on: a child plays it without an ocarina");
        static const uint32_t kNeeded[3] = { kXw4ButtonDown, kXw4ButtonUp, kXw4ButtonLeft };
        static const char* const kNames[3] = { "C-down", "C-up", "C-left" };
        for (int b = 0; b < 3; b++) {
            XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4AllButtons & ~kNeeded[b], 0) == 0,
                      "rule on: a child plays it without %s (Ocarina Button Shuffle must apply)", kNames[b]);
        }
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4SoaringButtons & ~(kXw4ButtonA | kXw4ButtonRight), 0) == 1,
                  "rule on: A or C-right was asked for (Soaring's notes are down, left, up)");
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 0, 0u, 1) == 1 && OoT_SoaringTest_Logic(0, 1, 1, kXw4AllButtons, 1) == 0,
                  "HasItem does not read exactly the song's flag");

        Xw4FreezeWorld(0u);
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4AllButtons, 0) == 0,
                  "rule off: a child holding the song plays it (only a world with the rule credits it)");
        ComboContext_Init();
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4AllButtons, 0) == 0, "nothing frozen: the song is playable");
    }

    // ---- (3) The logic's item effect: the flag, never a quest bit ---------------------
    {
        uint32_t quest = 0xFFFFFFFFu;
        const int applied = OoT_SoaringTest_ApplyEffect(&quest);
        XW4_CHECK(applied == 2 && quest == 0u,
                  "the effect answered %d with quest bits %08X: it must set and clear the song's flag and touch no "
                  "quest bit (the song's quest-table row is 0, the Forest Medallion)",
                  applied, (unsigned)quest);
    }

    // ---- (4) OoTMM's refusal and the option's warning ---------------------------------
    {
        char msg[256];
        const uint32_t modes[2] = { RSBS_SONG_SPOTS_SONG_LOCATIONS, RSBS_SONG_SPOTS_DUNGEON_REWARDS };
        const char* const modeNames[2] = { "Song Locations", "Dungeon Rewards" };
        for (int m = 0; m < 2; m++) {
            memset(msg, 'x', sizeof(msg));
            const uint32_t over = Combo_SongSpotsOverflow(13u, 12u, modes[m], true, msg, sizeof(msg));
            const std::string text(msg);
            XW4_CHECK(over == 1u && Xw4StartsWith(msg, "Not enough locations for songs/dungeon rewards") &&
                          text.find("13 songs for the 12 spots") != std::string::npos &&
                          text.find(modeNames[m]) != std::string::npos &&
                          text.find("Song of Soaring (OoT)") != std::string::npos &&
                          text.find("share a song or start with an Ocarina of Time song") != std::string::npos,
                      "13 on 12 under %s answered %u with '%s'", modeNames[m], (unsigned)over, msg);
            memset(msg, 'x', sizeof(msg));
            XW4_CHECK(Combo_SongSpotsOverflow(12u, 12u, modes[m], true, msg, sizeof(msg)) == 0u && msg[0] == '\0',
                      "12 on 12 under %s was refused ('%s')", modeNames[m], msg);
        }
        XW4_CHECK(Combo_SongSpotsOverflow(14u, 12u, RSBS_SONG_SPOTS_SONG_LOCATIONS, false, NULL, 0) == 2u,
                  "the overflow count is not songs minus spots");
        Combo_SongSpotsRecordRefusal("x");
        Combo_SongSpotsRecordRefusal(NULL);
        XW4_CHECK(Combo_SongSpotsLastRefusal() != NULL && Combo_SongSpotsLastRefusal()[0] == '\0',
                  "clearing the recorded refusal left '%s'", Combo_SongSpotsLastRefusal());
        const std::string tooltip(RSBS_SONG_SOARING_OOT_TOOLTIP);
        XW4_CHECK(tooltip.find("only as a child") != std::string::npos &&
                      tooltip.find("Song Locations") != std::string::npos &&
                      tooltip.find("Dungeon Rewards") != std::string::npos &&
                      tooltip.find("share a song or start with an Ocarina of Time song") != std::string::npos,
                  "the option text does not warn as OoTMM's does: '%s'", tooltip.c_str());
    }

    ComboContext_Init();
    printf("[TEST] oot-soaring-rule-logic: %s\n", sXw4Failures == 0 ? "PASS" : "FAIL");
    return sXw4Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// oot-soaring-rules-off-unchanged
// ============================================================================

TestResult Test_OoTSoaringRulesOffUnchanged(void) {
    printf("[TEST] oot-soaring-rules-off-unchanged: while the warp-song rules are hidden no world a player can make "
           "pools the Song of Soaring (OoT), credits it or meets the refusal, and every id before it is main's "
           "(#1169 XW4)\n");
    sXw4Failures = 0;
    Combo_ComboItemRules2RevealForTest(0);
    XW4_CHECK(OoT_ComboLogic_TestEnsureItemTableTransient() == 0, "OoT's item table is not available");

    // ---- (1) Hidden: Generate refuses both Soaring bits before anything freezes -------
    {
        XW4_CHECK((Combo_ComboItemRules2Refused(kXw4Soaring, NULL, 0) & kXw4Soaring) == kXw4Soaring &&
                      (Combo_ComboItemRules2Refused(kXw4Soaring | kXw4SharedSoaring, NULL, 0) &
                       (kXw4Soaring | kXw4SharedSoaring)) == (kXw4Soaring | kXw4SharedSoaring),
                  "a Soaring bit is not refused while the warp-song rules are hidden");
        XW4_CHECK(Combo_ResolveComboItemRules2() == 0u || !Combo_ComboSettingStoreAvailable(),
                  "the session resolves a second-word rule with no key set");
        const std::string play = Xw4ReadSource("games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp");
        const size_t gate = play.find("Combo_ComboItemRules2Refused(rsbsItemRules2");
        const size_t fill = play.find("int ret = Fill();");
        XW4_CHECK(gate != std::string::npos && fill != std::string::npos && gate < fill,
                  "from source, Generate does not refuse a hidden second-word rule before the fill (gate %zu, fill "
                  "%zu)",
                  gate, fill);
    }

    // ---- (2) A rules-off world: no copy, no credit ---------------------------------------
    {
        Xw4FreezeWorld(0u);
        XW4_CHECK(Combo_ComboCrossItemFlags2() == 0u && OoT_SongOfSoaringOoTPooledCopies() == 0 &&
                      !OoT_SongOfSoaringOoTOn(),
                  "a rules-off world pools %d copies of the song", OoT_SongOfSoaringOoTPooledCopies());
        XW4_CHECK(OoT_SoaringTest_Logic(1, 1, 1, kXw4AllButtons, 0) == 0,
                  "a rules-off world credits the song to a child who somehow holds it");
    }

    // ---- (3) The refusal never meets today's song pass ---------------------------------
    {
        char msg[64];
        for (uint32_t songs = 0; songs <= 12u; songs++) {
            XW4_CHECK(
                Combo_SongSpotsOverflow(songs, 12u, RSBS_SONG_SPOTS_SONG_LOCATIONS, false, msg, sizeof(msg)) == 0u &&
                    Combo_SongSpotsOverflow(songs, 12u, RSBS_SONG_SPOTS_DUNGEON_REWARDS, false, msg, sizeof(msg)) == 0u,
                "%u of OoT's own songs on its 12 spots were refused", (unsigned)songs);
        }
    }

    // ---- (4) Main's ids for everything before it ------------------------------------------
    {
        // Computed outside the build from main's randomizerTypes.h and
        // randomizer_inf.h: the Stone Mask's three ids, which the song follows.
        XW4_CHECK(OoT_SoaringTest_Id(0) == 303 && OoT_SoaringTest_PoolId() == 304,
                  "RG_STONE_MASK is %d (main's 303) and the song %d (304): a placed id moved", OoT_SoaringTest_Id(0),
                  OoT_SoaringTest_PoolId());
        XW4_CHECK(OoT_SoaringTest_Id(8) == 1514 && OoT_SoaringTest_Id(7) == 1515,
                  "RHT_STONE_MASK is %d (main's 1514) and the song's key %d (1515)", OoT_SoaringTest_Id(8),
                  OoT_SoaringTest_Id(7));
        // RAND_INF_MAX is not pinned: a later flag appends after the song's.
        XW4_CHECK(OoT_SoaringTest_Id(4) == 2061 && OoT_SoaringTest_Id(3) == 2062,
                  "the Stone Mask's flag is %d (main's 2061) and the song's %d (2062)", OoT_SoaringTest_Id(4),
                  OoT_SoaringTest_Id(3));
    }

    // ---- (5) From source: the pool and the song pass ask the rule ----------------------------
    {
        const std::string pool = Xw4ReadSource("games/oot/soh/Enhancements/randomizer/3drando/item_pool.cpp");
        const size_t ask = pool.find("if (OoT_SongOfSoaringOoTPooledCopies() > 0) {");
        const size_t add = pool.find("AddItemToPool(RG_SONG_OF_SOARING_OOT");
        XW4_CHECK(ask != std::string::npos && add != std::string::npos && ask < add &&
                      Xw4Count(pool, "RG_SONG_OF_SOARING_OOT") == 1,
                  "item_pool.cpp adds the song other than once, under the frozen rule");
        const std::string fill = Xw4ReadSource("games/oot/soh/Enhancements/randomizer/3drando/fill.cpp");
        const size_t holdBack = fill.find("RsbsHoldBackSharedSongs(songs);");
        const size_t refuse = fill.find("OoT_SongSpots_RefuseOverflow(songs, songLocations.size()");
        const size_t place = fill.find("AssumedFill(songs, songLocations, true);");
        XW4_CHECK(holdBack != std::string::npos && refuse != std::string::npos && place != std::string::npos &&
                      holdBack < refuse && refuse < place,
                  "fill.cpp does not ask the refusal after the #1085 hold-back and before placing the songs (%zu, "
                  "%zu, %zu)",
                  holdBack, refuse, place);
    }

    ComboContext_Init();
    printf("[TEST] oot-soaring-rules-off-unchanged: %s\n", sXw4Failures == 0 ? "PASS" : "FAIL");
    return sXw4Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// The rando rows: real paired OoT generations
// ============================================================================

namespace {

const char* const kXw4ShuffleSongsKey = "gRandoSettings.ShuffleSongs";
const char* const kXw4ItemPoolKey = "gRandoSettings.ItemPool";
const char* const kXw4StartingLullabyKey = "gRandoSettings.StartingZeldasLullaby";
const int kXw4SongsOff = 0;
const int kXw4SongLocations = 1;
const int kXw4DungeonRewards = 2;
const int kXw4Anywhere = 3;
const int kXw4ItemPoolBalanced = 1;
const char* const kXw4ShuffleNames[4] = { "Off", "Song Locations", "Dungeon Rewards", "Anywhere" };

/** One leg's settings, put back by Xw4ClearLeg. */
struct Xw4Leg {
    int shuffleSongs;
    bool soaring;       // songSoaringOot
    bool sharedSoaring; // sharedSongSoaring
    bool sharedTime;    // the first word's Shared Song of Time (#1085's hold-back)
    bool startLullaby;  // a starting song
};

void Xw4ClearLeg() {
    CVarClear(kXw4ShuffleSongsKey);
    CVarClear(kXw4ItemPoolKey);
    CVarClear(kXw4StartingLullabyKey);
    CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SONG_SOARING_OOT));
    CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SHARED_SONG_SOARING));
    CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SHARED_SONG_TIME));
    Combo_ComboItemRules2RevealForTest(0);
}

/** Generate OoT's half of a paired world under @p leg, through the test seam:
 *  the bridge's return (0 generated). */
int Xw4Generate(const Xw4Leg& leg, const char* seed) {
    ComboContext_Init();
    Xw4ClearLeg();
    Combo_SongSpotsRecordRefusal(NULL);
    CVarSetInteger(kXw4ShuffleSongsKey, leg.shuffleSongs);
    CVarSetInteger(kXw4ItemPoolKey, kXw4ItemPoolBalanced);
    if (leg.startLullaby) {
        CVarSetInteger(kXw4StartingLullabyKey, 1);
    }
    if (leg.soaring) {
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_SONG_SOARING_OOT), 1);
    }
    if (leg.sharedSoaring) {
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_SHARED_SONG_SOARING), 1);
    }
    if (leg.sharedTime) {
        CVarSetInteger(Combo_ComboSettingKey(COMBO_SETTING_SHARED_SONG_TIME), 1);
    }
    // THE TEST SEAM: the hidden warp-song rules read as offered, as they will
    // once lane XW12 lands, so Generate takes a world with one on. Production
    // never calls it.
    Combo_ComboItemRules2RevealForTest(leg.soaring || leg.sharedSoaring ? 1 : 0);
    const int rc = Rando_HeadlessSeedTest(seed);
    Combo_ComboItemRules2RevealForTest(0);
    return rc;
}

/** Every key a leg may have set is cleared when the row ends, however it ends. */
struct Xw4Restore {
    ~Xw4Restore() {
        Xw4ClearLeg();
        Combo_SongSpotsRecordRefusal(NULL);
        ComboContext_Init();
    }
};

} // namespace

TestResult OoTSoaringPooledOnlyWhenRuleOn_Run(void) {
    printf("[TEST] oot-soaring-pooled-only-when-rule-on: a real paired OoT generation pools no Song of Soaring (OoT) "
           "with the rule off, exactly one with it on (through the test seam), and none with the shared song "
           "(#1169 XW4)\n");
    sXw4Failures = 0;
    Xw4Restore restore;

    // ---- Rule off: the shipped world ----------------------------------------------------
    {
        const Xw4Leg leg = { kXw4SongLocations, false, false, false, false };
        const int rc = Xw4Generate(leg, "RSBSXW4SOARING0");
        const int copies = OoT_SoaringTest_WorldCopies();
        printf("[TEST] rule off (Song Locations): generation %d, %d copies, second word %08X\n", rc, copies,
               (unsigned)Combo_ComboCrossItemFlags2());
        XW4_CHECK(rc == 0, "the rules-off generation failed");
        XW4_CHECK(copies == 0 && Combo_ComboCrossItemFlags2() == 0u,
                  "with the rule off OoT's world holds %d copies of the song", copies);
    }

    // ---- Rule on: exactly one copy, whatever the unrestricted Shuffle Songs -------------
    {
        const int modes[2] = { kXw4Anywhere, kXw4SongsOff };
        for (int m = 0; m < 2; m++) {
            const Xw4Leg leg = { modes[m], true, false, false, false };
            const int rc = Xw4Generate(leg, "RSBSXW4SOARING1");
            const int copies = OoT_SoaringTest_WorldCopies();
            printf("[TEST] rule on (%s): generation %d, %d copies, second word %08X\n", kXw4ShuffleNames[modes[m]], rc,
                   copies, (unsigned)Combo_ComboCrossItemFlags2());
            XW4_CHECK(rc == 0, "the rule-on generation under %s failed", kXw4ShuffleNames[modes[m]]);
            XW4_CHECK(Combo_ComboCrossItemFlags2() == kXw4Soaring, "the creation froze %08X, not songSoaringOot alone",
                      (unsigned)Combo_ComboCrossItemFlags2());
            XW4_CHECK(copies == 1, "with the rule on under %s OoT's world holds %d copies, not exactly one",
                      kXw4ShuffleNames[modes[m]], copies);
        }
        XW4_CHECK(OoT_SoaringTest_HintNamesSong() == 1, "the song's hint text does not name it");
        XW4_CHECK(OoT_SoaringTest_RealGive() == 1,
                  "OoT's real give did not set the song's flag alone (a quest bit or a slot moved)");
    }

    // ---- Rule and shared rule on: the family path, held back ------------------------------
    {
        const Xw4Leg leg = { kXw4Anywhere, true, true, false, false };
        const int rc = Xw4Generate(leg, "RSBSXW4SOARING2");
        const int copies = OoT_SoaringTest_WorldCopies();
        printf("[TEST] shared Soaring (Anywhere): generation %d, %d copies, second word %08X\n", rc, copies,
               (unsigned)Combo_ComboCrossItemFlags2());
        XW4_CHECK(rc == 0, "the shared-Soaring generation failed");
        XW4_CHECK(Combo_ComboCrossItemFlags2() == (kXw4Soaring | kXw4SharedSoaring),
                  "the creation froze %08X, not both Soaring bits", (unsigned)Combo_ComboCrossItemFlags2());
        XW4_CHECK(copies == 0,
                  "with the shared Soaring OoT's world holds %d separate copies: it must be held back, never placed",
                  copies);
    }

    printf("[TEST] oot-soaring-pooled-only-when-rule-on: %s\n", sXw4Failures == 0 ? "PASS" : "FAIL");
    return sXw4Failures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SongLocationsOverflowRefused_Run(void) {
    printf("[TEST] song-locations-overflow-refused: a separate Song of Soaring (OoT) makes 13 songs for the 12 spots "
           "of Song Locations or Dungeon Rewards, and Generate refuses with OoTMM's wording; sharing or starting with "
           "a song, or Anywhere, generates (#1169 XW4)\n");
    sXw4Failures = 0;
    Xw4Restore restore;

    // ---- Refused: 13 on 12 in either restricted mode ---------------------------------------
    {
        const int modes[2] = { kXw4SongLocations, kXw4DungeonRewards };
        for (int m = 0; m < 2; m++) {
            const Xw4Leg leg = { modes[m], true, false, false, false };
            const int rc = Xw4Generate(leg, "RSBSXW4OVERFLOW");
            const char* refusal = Combo_SongSpotsLastRefusal();
            printf("[TEST] 13 on 12 (%s): generation %d, refusal '%s'\n", kXw4ShuffleNames[modes[m]], rc, refusal);
            XW4_CHECK(rc != 0, "13 songs on the 12 spots of %s generated", kXw4ShuffleNames[modes[m]]);
            XW4_CHECK(Xw4StartsWith(refusal, RSBS_SONG_SPOTS_REFUSAL) &&
                          strstr(refusal, "13 songs for the 12 spots") != NULL &&
                          strstr(refusal, kXw4ShuffleNames[modes[m]]) != NULL &&
                          strstr(refusal, "Song of Soaring (OoT)") != NULL,
                      "under %s the generation was not refused with OoTMM's wording ('%s')", kXw4ShuffleNames[modes[m]],
                      refusal);
            XW4_CHECK(!Combo_ComboSettingsFrozen(),
                      "the refused creation left a frozen identity behind (no partial identity, #564)");
        }
    }

    // ---- Generated: every way OoTMM's text names, and Anywhere -------------------------------
    {
        struct Pass {
            Xw4Leg leg;
            const char* name;
            int onSongLocations; // copies the song pass placed on a song check, or -1 to skip
        };
        const Pass passes[] = {
            { { kXw4SongLocations, true, false, false, true }, "Song Locations, starting with Zelda's Lullaby", 1 },
            { { kXw4SongLocations, true, false, true, false }, "Song Locations, Shared Song of Time", 1 },
            { { kXw4SongLocations, true, true, false, false }, "Song Locations, Shared Song of Soaring", 0 },
            { { kXw4DungeonRewards, true, false, false, true }, "Dungeon Rewards, starting with Zelda's Lullaby", -1 },
            { { kXw4Anywhere, true, false, false, false }, "Anywhere", -1 },
            { { kXw4SongLocations, false, false, false, false }, "Song Locations, rule off", 0 },
        };
        for (const Pass& p : passes) {
            const int rc = Xw4Generate(p.leg, "RSBSXW4OVERFLOW");
            const char* refusal = Combo_SongSpotsLastRefusal();
            const int onSongs = OoT_SoaringTest_OnSongLocations();
            printf("[TEST] %s: generation %d, refusal '%s', %d on song checks\n", p.name, rc, refusal, onSongs);
            XW4_CHECK(rc == 0 && refusal[0] == '\0', "%s did not generate ('%s')", p.name, refusal);
            if (p.onSongLocations >= 0) {
                XW4_CHECK(onSongs == p.onSongLocations, "%s put %d copies on song checks, expected %d", p.name, onSongs,
                          p.onSongLocations);
            }
        }
    }

    printf("[TEST] song-locations-overflow-refused: %s\n", sXw4Failures == 0 ? "PASS" : "FAIL");
    return sXw4Failures == 0 ? TEST_PASS : TEST_FAIL;
}
