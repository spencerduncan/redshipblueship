/**
 * @file mm_creation_new_file_test.cpp
 * @brief The paired creation event authors MM's half the way MM's own
 *        file-select new-file path would (#765).
 *
 * ============================================================================
 * WHAT WAS WRONG
 * ============================================================================
 *
 * MM_Rando_AuthorHalfAtCreation memsets the whole SaveContext, runs
 * MM_Sram_InitNewSave and MM's generation, and arms the result as the MM
 * shadow. MM's own file select (MM_Sram_InitSave, z_sram_NES.c) does the same
 * InitNewSave but then stamps the 'ZELDA3' new-file marker and the checksum,
 * and it runs in a context whose fileNum and flashSaveAvailable the title chain
 * and the naming screen set. The creation event stamped none of them, so the
 * armed half reached every reader with:
 *   - newf all zero: the .redsave panel's "started" flag and the Combo Tracker's
 *     MM presence gate both read "no MM save" (#765, found by #755);
 *   - fileNum 0 and flashSaveAvailable 0: the arrival consumes the WHOLE
 *     SaveContext from the shadow, so these replaced the cross-game session's
 *     0xFF sentinel and its "flash available" for the life of the file. fileNum
 *     0 is a real flash slot, so the moon-crash reset reloaded slot 0 through
 *     the single-exe read stub (which returns nothing) and copied the zeroed
 *     buffer over the live save.
 * And MM's slot-metadata descriptor was never registered in this binary at all
 * (its only registrar lives in the excluded 2s2h/SaveManager/SaveManager.cpp),
 * so every slot's MM half read "not started" whatever its bytes held.
 *
 * ============================================================================
 * TWO ROWS
 * ============================================================================
 *
 *   mm-creation-new-file (redship tier, ROM-free). A SYNTHETIC authoring: the
 *   creation event's own steps minus the generation (memset, MM_Sram_InitNewSave,
 *   the production stamp MM_Creation_StampNewFileFields, a stand-in for
 *   OnFileCreate's rando stamp), armed by the production MM_Rando_ArmCreatedHalf.
 *   Checks the armed shadow's marker, checksum parity, fileNum and flash flag;
 *   the Combo Tracker reads it present by the marker ALONE (a rando-typed shadow
 *   without the marker reads UNAVAILABLE: #755's widening is gone), labelled "As
 *   of file creation" until MM's first load stamps fileCreatedAt; the .redsave
 *   slot panel reports MM started through MM_SlotMeta_Register (and not for an
 *   all-zero MM half); and the moon-crash reset over the consumed half keeps
 *   the save.
 *
 *   mm-creation-new-file-world (rando tier). The PRODUCTION creation event
 *   (OoT_Creation_AuthorRandoFile) over the ComboSingleBag pinned seed
 *   RSBSSINGLEBAG1, then the same checks over the armed shadow it leaves.
 *
 * ============================================================================
 * THE NAME (#773), IN ALL THREE ROWS
 * ============================================================================
 *
 * MM's own new-file path also copies the name typed on MM's naming screen; the
 * creation event had no name to copy, so every paired half kept MM's default,
 * eight MM spaces, and every MM textbox that names Link printed nothing. The
 * paired world has one name: the creation translates OoT's typed name into MM's
 * charset (OoT_PlayerName_ToMMCharset, OoTMM's copyName mapping) and stamps it
 * where MM_Sram_InitSave stamps the typed name. Both rows above author with a
 * synthetic OoT name (NTSC English "Ab1 z-." plus a voicing mark, which has no
 * MM glyph) and check the half's bytes and the slot panel's decoded name; the
 * synthetic row's byte parity with MM_Sram_InitSave now INCLUDES the name.
 *
 *   combo-player-name (redship tier, ROM-free). The translation itself, in each
 *   of OoT's filename charsets, against explicit expected MM bytes; every byte
 *   of every charset lands on an MM glyph; every character OoT's own printer can
 *   show prints the same in MM's printer; and the panel's MM decoder.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "combo_tracker_view.h"
#include "context.h"
#include "game.h"
#include "save.h"

extern "C" {
void MM_Creation_StampNewFileFields(const u8* playerName);
void OoT_PlayerName_ToMMCharset(const uint8_t* ootName, uint8_t filenameLanguage, uint8_t* mmName);
void Randomizer_TestSetOoTPlayerName(const uint8_t* name, uint8_t filenameLanguage);
void OoT_SlotMeta_DecodePlayerName(const uint8_t* blob, size_t blobSize, char outName[9]);
void MM_SlotMeta_Register(void);
int MM_Rando_ArmCreatedHalf(int slot);
int Combo_ConsumeFrozenState(const char* gameId, void* saveContext, size_t size);
int OoT_Creation_AuthorRandoFile(int slot);
int Rando_HeadlessSeedTest(const char* seedStr);
void Randomizer_TestClearOoTSave(void);
void Combo_SingleBag_Forget(void);
void Combo_Crossings_Clear(void);
}

namespace {

#define CNF_ASSERT(cond, msg)                                                                     \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            printf("[TEST] FAIL (%s): %s  [%s] (%s:%d)\n", sRow, msg, #cond, __FILE__, __LINE__); \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

const char* sRow = "mm-creation-new-file";
const char* const kSaveDir = "rsbs_test_saves_creation_new_file";
const u8 kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };

// OoT's filename languages (soh/SaveManager.h FilenameLanguage; OoT's header is
// not includable from an MM TU).
constexpr uint8_t kLangPal = 0;
constexpr uint8_t kLangNtscJpn = 1;
constexpr uint8_t kLangNtscEng = 2;

// The name the two creation rows type on OoT's file select: NTSC English
// "Ab1 z-." and, last, a voicing mark (0xE7), which has no MM glyph.
const uint8_t kOoTName[8] = { 0xAB, 0xC6, 0x01, 0xDF, 0xDE, 0xE4, 0xEA, 0xE7 };
// ...and what MM's half must carry: A b 1 space z - . space(no glyph).
const u8 kMMName[8] = { 0x0A, 0x25, 0x01, 0x3E, 0x3D, 0x3F, 0x40, 0x3E };
// ...and what the slot panel prints (trailing space dropped).
const char* const kPanelName = "Ab1 z-.";
const u8 kMMDefaultName[8] = { 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E };

void PrintName(const char* what, const u8* name) {
    printf("[TEST] %s: %s: %02X %02X %02X %02X %02X %02X %02X %02X\n", sRow, what, name[0], name[1], name[2], name[3],
           name[4], name[5], name[6], name[7]);
}

// The flash funnel's saveBuf and a copy of the armed half. Static: 128 KiB and
// ~64 KiB do not belong on the test's stack.
u8 sSaveBuf[SAVE_BUFFER_SIZE];
SaveContext sHalf;

void ReadArmedHalf() {
    memcpy(&sHalf, Context_GetMMSaveContext(), sizeof(SaveContext));
}

void PrintHalf(const char* when) {
    const u8* n = reinterpret_cast<const u8*>(sHalf.save.saveInfo.playerData.newf);
    printf("[TEST] %s: %s: newf=%02X %02X %02X %02X %02X %02X saveType=%d fileNum=0x%X flashSaveAvailable=%d "
           "checksum=0x%04X fileCreatedAt=%llu finalSeed=%08X\n",
           sRow, when, n[0], n[1], n[2], n[3], n[4], n[5], (int)sHalf.save.shipSaveInfo.saveType,
           (unsigned)sHalf.fileNum, (int)sHalf.flashSaveAvailable, (unsigned)sHalf.save.saveInfo.checksum,
           (unsigned long long)sHalf.save.shipSaveInfo.fileCreatedAt,
           (unsigned)sHalf.save.shipSaveInfo.rando.finalSeed);
}

/** The MM half in the shadow, overwritten with `bytes` (tracker/meta probes). */
void PutShadow(const std::vector<uint8_t>& bytes) {
    Context_UpdateShadowCopy(GAME_MM, bytes.data(), bytes.size());
}

/**
 * Everything both rows check about the armed half the creation left in the MM
 * shadow. `expectSeed` is the half's MM final seed.
 */
int CheckArmedHalf(uint32_t expectSeed) {
    ReadArmedHalf();
    PrintHalf("the armed MM half");

    // ---- 1. the fields MM's own new-file path stamps ------------------------
    CNF_ASSERT(memcmp(sHalf.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0,
               "the armed half carries MM's 'ZELDA3' new-file marker");
    CNF_ASSERT(sHalf.save.shipSaveInfo.saveType == SAVETYPE_RANDO, "the armed half is a rando save");
    CNF_ASSERT(sHalf.fileNum == 0xFF, "the armed half carries the cross-game 0xFF fileNum sentinel, not slot 0");
    CNF_ASSERT(sHalf.flashSaveAvailable, "the armed half carries the cross-game session's flashSaveAvailable");
    CNF_ASSERT(sHalf.save.shipSaveInfo.fileCreatedAt == 0, "MM has not loaded the armed half: no creation stamp yet");
    PrintName("the armed half's playerName", reinterpret_cast<const u8*>(sHalf.save.saveInfo.playerData.playerName));
    CNF_ASSERT(memcmp(sHalf.save.saveInfo.playerData.playerName, kMMName, sizeof(kMMName)) == 0,
               "the armed half carries OoT's typed name in MM's charset, not MM's all-space default (#773)");

    // ---- 2. the Combo Tracker: present by the marker alone -------------------
    MM_TrackerAdapter_Register();
    const ComboMMTrackerDesc* desc = Combo_Tracker_GetMMDesc();
    CNF_ASSERT(desc != nullptr, "MM's tracker descriptor registered");
    ComboTrackerGameSummary summary;
    Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
    printf("[TEST] %s: tracker MM summary: freshness=%d hasWorld=%d seed=%08X shuffled=%d label=\"%s\"\n", sRow,
           (int)summary.freshness, (int)summary.hasWorld, (unsigned)summary.seed, summary.shuffled,
           Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE));
    CNF_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_STALE, "the tracker reads the armed MM half as present");
    CNF_ASSERT(summary.hasWorld && summary.seed == expectSeed, "the tracker reads the armed half's rando world");
    CNF_ASSERT(
        strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE), "As of file creation") == 0,
        "a half MM has never loaded is labelled as of file creation");

    std::vector<uint8_t> armed((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    memcpy(armed.data(), Context_GetMMSaveContext(), armed.size());
    {
        // #755's widening is gone: a rando save type without the marker is not
        // an MM save (the shape every creation left before #765).
        std::vector<uint8_t> unmarked = armed;
        memset(unmarked.data() + desc->newfOffset, 0, desc->newfLen);
        PutShadow(unmarked);
        Combo_TrackerGameSummary((uint8_t)GAME_MM, &summary);
        CNF_ASSERT(summary.freshness == COMBO_TRACKER_FRESH_UNAVAILABLE,
                   "presence is the marker's alone: no save-type widening");
        // MM's first load stamps fileCreatedAt (SavingEnhancements' OnSaveLoad);
        // from then on the note names the last switch or save.
        std::vector<uint8_t> loaded = armed;
        const uint64_t createdAt = 1760000000ull;
        memcpy(loaded.data() + offsetof(SaveContext, save.shipSaveInfo.fileCreatedAt), &createdAt, sizeof(createdAt));
        PutShadow(loaded);
        CNF_ASSERT(strcmp(Combo_TrackerFreshnessLabel((uint8_t)GAME_MM, COMBO_TRACKER_FRESH_STALE),
                          "As of the last game switch or save") == 0,
                   "a half MM has loaded is labelled as of the last switch or save");
        PutShadow(armed);
    }

    // ---- 3. the .redsave slot panel: MM started -----------------------------
    MM_SlotMeta_Register();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveDir);
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(0); // an erase is what unlatches the slot for this session's write
    CNF_ASSERT(mgr.Save(0), "the slot commits");
    rsbs::SlotMeta meta = mgr.ReadMeta(0);
    printf("[TEST] %s: slot meta: valid=%d ootStarted=%d mmStarted=%d mmName=\"%s\"\n", sRow, (int)meta.valid,
           (int)meta.ootStarted, (int)meta.mmStarted, meta.mmName);
    CNF_ASSERT(meta.valid && meta.mmStarted, "the slot panel reads the paired MM half as started");
    CNF_ASSERT(strcmp(meta.mmName, kPanelName) == 0,
               "the slot panel names MM's half by the one name, decoded from MM's charset (#773)");
    {
        // Control: an MM half that does not exist reads not started.
        std::vector<uint8_t> zero((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        PutShadow(zero);
        mgr.DeleteSave(0);
        CNF_ASSERT(mgr.Save(0), "the control slot commits");
        meta = mgr.ReadMeta(0);
        CNF_ASSERT(meta.valid && !meta.mmStarted, "an all-zero MM half reads not started");
        PutShadow(armed);
    }
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
    return 0;
}

/**
 * The consequence the fileNum stamp exists for: the arrival consumes the armed
 * half into the live save, and the moon-crash reset must not reload a flash slot
 * the session does not have (Sram_FileNumHasFlashSlot) and copy the read stub's
 * empty buffer over it.
 */
int CheckMoonCrashKeepsConsumedHalf() {
    CNF_ASSERT(Combo_ConsumeFrozenState("mm", &gSaveContext, sizeof(gSaveContext)) == 1,
               "the armed half is consumed into the live save, as the arrival does");
    const s32 day = 2;
    const s16 rupees = 0x77;
    gSaveContext.save.day = day;
    gSaveContext.save.saveInfo.playerData.rupees = rupees;
    gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;

    SramContext sramCtx;
    memset(&sramCtx, 0, sizeof(sramCtx));
    memset(sSaveBuf, 0, sizeof(sSaveBuf));
    sramCtx.saveBuf = sSaveBuf;
    Sram_ResetSaveFromMoonCrash(&sramCtx);

    printf("[TEST] %s: after the moon-crash reset: fileNum=0x%X day=%d rupees=%d bottle1=0x%02X newf[0]=%02X\n", sRow,
           (unsigned)gSaveContext.fileNum, (int)gSaveContext.save.day,
           (int)gSaveContext.save.saveInfo.playerData.rupees,
           (unsigned)gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1],
           (unsigned)gSaveContext.save.saveInfo.playerData.newf[0]);
    CNF_ASSERT(gSaveContext.save.day == day && gSaveContext.save.saveInfo.playerData.rupees == rupees,
               "the moon-crash reset keeps the consumed half's save (no reload from a flash slot it does not have)");
    CNF_ASSERT(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] == ITEM_BOTTLE,
               "the moon-crash reset keeps the consumed half's inventory");
    CNF_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.newf, kNewf, sizeof(kNewf)) == 0,
               "the moon-crash reset keeps the marker");
    return 0;
}

} // namespace

extern "C" int MM_CreationNewFile_RunSynthetic(void) {
    sRow = "mm-creation-new-file";
    printf("[TEST] %s: a synthetic creation-authored MM half carries what MM's own new-file path stamps (#765)\n",
           sRow);

    const GameId prevGame = Context_GetCurrentGame();
    Context_InitFrozenStates();
    ComboContext_Init();
    Context_ClearAllFrozenStates();

    // The creation event's own authoring, minus the generation it runs between
    // these lines (MM_Rando_AuthorHalfAtCreation).
    const uint32_t kSeed = 0x5EED0765u;
    memset(&gSaveContext, 0, sizeof(SaveContext));
    MM_Sram_InitNewSave();
    // MM_Sram_InitSave's own steps over the SAME bytes (InitNewSave draws its
    // random save fields -- lottery, bomber and spider-house codes -- from the
    // RNG, so a second InitNewSave is a different Save): the typed name (here,
    // the name the player typed on OoT's file select, in MM's charset: #773),
    // the 'ZELDA3' marker, then the checksum over Save. Button 0's cutsceneIndex
    // is the one InitSave step the creation event does not take (see
    // MM_Creation_StampNewFileFields's header), so it is left out here too.
    CNF_ASSERT(memcmp(gSaveContext.save.saveInfo.playerData.playerName, kMMDefaultName, 8) == 0,
               "MM_Sram_InitNewSave authors MM's all-space default name");
    uint8_t typed[8];
    OoT_PlayerName_ToMMCharset(kOoTName, kLangNtscEng, typed);
    PrintName("OoT's typed name in MM's charset", typed);
    CNF_ASSERT(memcmp(typed, kMMName, sizeof(kMMName)) == 0, "OoT's typed name translates into MM's charset");
    static Save sMMPath;
    memcpy(&sMMPath, &gSaveContext.save, sizeof(Save));
    memcpy(sMMPath.saveInfo.playerData.playerName, kMMName, sizeof(kMMName));
    memcpy(sMMPath.saveInfo.playerData.newf, kNewf, sizeof(kNewf));
    sMMPath.saveInfo.checksum = Sram_CalcChecksum(&sMMPath, sizeof(Save));

    MM_Creation_StampNewFileFields(typed);
    printf("[TEST] %s: checksum stamped=0x%04X MM's path=0x%04X\n", sRow, (unsigned)gSaveContext.save.saveInfo.checksum,
           (unsigned)sMMPath.saveInfo.checksum);
    CNF_ASSERT(gSaveContext.save.saveInfo.checksum == sMMPath.saveInfo.checksum,
               "the checksum is the one MM_Sram_InitSave computes");
    CNF_ASSERT(memcmp(&gSaveContext.save, &sMMPath, sizeof(Save)) == 0,
               "the stamped Save is byte-identical to MM_Sram_InitSave's, name included (button-0 cutscene aside)");
    // Stand-in for OnFileCreate's rando block (the generation's first stamp).
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    gSaveContext.save.shipSaveInfo.rando.finalSeed = kSeed;

    CNF_ASSERT(MM_Rando_ArmCreatedHalf(0) == 0, "the authored half arms");
    CNF_ASSERT(Context_HasFrozenState(GAME_MM) != 0, "the MM shadow is armed");
    Context_SetCurrentGame(GAME_OOT);

    if (CheckArmedHalf(kSeed) != 0) {
        return 1;
    }
    if (CheckMoonCrashKeepsConsumedHalf() != 0) {
        return 1;
    }

    Context_ClearAllFrozenStates();
    ComboContext_Init();
    memset(&gSaveContext, 0, sizeof(SaveContext));
    Context_SetCurrentGame(prevGame);
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

extern "C" int MM_CreationNewFile_RunWorld(void) {
    sRow = "mm-creation-new-file-world";
    printf("[TEST] %s: the production paired creation over RSBSSINGLEBAG1 arms an MM half MM's own new-file path "
           "would recognize (#765)\n",
           sRow);

    CNF_ASSERT(Rando_HeadlessSeedTest("RSBSSINGLEBAG1") == 0, "the pinned seed generates");
    Randomizer_TestClearOoTSave();
    // What OoT's file select writes before the creation seam (Sram_InitSave).
    Randomizer_TestSetOoTPlayerName(kOoTName, kLangNtscEng);
    Context_ClearFrozenState(GAME_MM);
    CNF_ASSERT(OoT_Creation_AuthorRandoFile(0) == 1, "the production creation event succeeds");
    CNF_ASSERT(Context_HasFrozenState(GAME_MM) != 0, "MM's world is in the armed shadow; MM never booted");
    Context_SetCurrentGame(GAME_OOT);

    ReadArmedHalf();
    const uint32_t seed = sHalf.save.shipSaveInfo.rando.finalSeed;
    CNF_ASSERT(seed != 0, "the armed half carries a generated world");
    if (CheckArmedHalf(seed) != 0) {
        return 1;
    }
    if (CheckMoonCrashKeepsConsumedHalf() != 0) {
        return 1;
    }

    Combo_SingleBag_Forget();
    Combo_Crossings_Clear();
    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}

namespace {

/** What OoT's own name printer shows for one name byte (z_message_PAL.c's
 *  MESSAGE_NAME decode, non-JPN text): '?' for a byte it shows as a blank or a
 *  kana glyph MM cannot draw, which the translation must turn into MM's space. */
char OoTPrints(uint8_t c, uint8_t lang) {
    if (lang == kLangPal) {
        if (c == 0x3E) {
            return ' ';
        } else if (c == 0x40) {
            return '.';
        } else if (c == 0x3F) {
            return '-';
        } else if (c < 0x0A) {
            return (char)('0' + c);
        } else if (c < 0x24) {
            return (char)(c + '7'); // 0x0A + '7' == 'A'
        } else if (c < 0x3E) {
            return (char)(c + '='); // 0x24 + '=' == 'a'
        }
        return '?';
    }
    if (c == 0xDF) {
        return ' ';
    } else if (c == 0xEA) {
        return '.';
    } else if (c == 0xE4) {
        return '-';
    } else if (c < 0x0A) {
        return (char)('0' + c);
    } else if (c >= 0xAB && c < 0xC5) {
        return (char)(c - 0x6A);
    } else if (c >= 0xC5 && c < 0xDF) {
        return (char)(c - 0x64);
    }
    return '?';
}

/** What MM's name printer shows for one byte (z_message_nes.c MESSAGE_NAME). */
char MMPrints(uint8_t c) {
    if (c == 0x3E) {
        return ' ';
    } else if (c == 0x40) {
        return '.';
    } else if (c == 0x3F) {
        return '-';
    } else if (c < 0x0A) {
        return (char)('0' + c);
    } else if (c < 0x24) {
        return (char)('A' - 10 + c);
    } else if (c < 0x3E) {
        return (char)('a' - 36 + c);
    }
    return '?';
}

struct NameCase {
    const char* what;
    uint8_t lang;
    uint8_t oot[8];
    uint8_t mm[8];
};

// Expected MM bytes, written out by hand from the two charsets (never computed
// by the code under test).
const NameCase kCases[] = {
    // "Link" typed on NTSC English: MM's "LINK" default is 15 12 17 14.
    { "NTSC English \"Link\"",
      kLangNtscEng,
      { 0xB6, 0xCD, 0xD2, 0xCF, 0xDF, 0xDF, 0xDF, 0xDF },
      { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E } },
    // Letters at both ends of both cases, digits 0 and 9, space, '-', '.'.
    { "NTSC English \"Zaz09-. \"",
      kLangNtscEng,
      { 0xC4, 0xC5, 0xDE, 0x00, 0x09, 0xE4, 0xEA, 0xDF },
      { 0x23, 0x24, 0x3D, 0x00, 0x09, 0x3F, 0x40, 0x3E } },
    // The creation rows' name, with the voicing mark (0xE7: no MM glyph).
    { "NTSC English \"Ab1 z-.\" + voicing mark",
      kLangNtscEng,
      { 0xAB, 0xC6, 0x01, 0xDF, 0xDE, 0xE4, 0xEA, 0xE7 },
      { 0x0A, 0x25, 0x01, 0x3E, 0x3D, 0x3F, 0x40, 0x3E } },
    // NTSC Japanese: hiragana A (0x0A) and katakana A (0x5A) have no MM glyph;
    // the alphanumeric page's 'A', the long-vowel mark (same byte as '-'), both
    // voicing marks, a digit, space.
    { "NTSC Japanese kana + Latin",
      kLangNtscJpn,
      { 0x0A, 0x5A, 0xAB, 0xE4, 0xE7, 0xE8, 0x09, 0xDF },
      { 0x3E, 0x3E, 0x0A, 0x3F, 0x3E, 0x3E, 0x09, 0x3E } },
    // PAL: the PAL charset is MM's, so every PAL name byte carries over as is;
    // 0x41 and 0xDF are not PAL name bytes and become MM's space.
    { "PAL \"Aa0 -.\" + two non-PAL bytes",
      kLangPal,
      { 0x0A, 0x24, 0x00, 0x3E, 0x3F, 0x40, 0x41, 0xDF },
      { 0x0A, 0x24, 0x00, 0x3E, 0x3F, 0x40, 0x3E, 0x3E } },
};

} // namespace

extern "C" int MM_CreationPlayerName_Run(void) {
    sRow = "combo-player-name";
    printf("[TEST] %s: OoT's typed name becomes the paired MM half's name in MM's charset (#773)\n", sRow);

    // ---- 1. hand-written cases ----------------------------------------------
    for (const NameCase& c : kCases) {
        uint8_t mm[8];
        memset(mm, 0xAA, sizeof(mm));
        OoT_PlayerName_ToMMCharset(c.oot, c.lang, mm);
        printf("[TEST] %s: %s -> %02X %02X %02X %02X %02X %02X %02X %02X\n", sRow, c.what, mm[0], mm[1], mm[2], mm[3],
               mm[4], mm[5], mm[6], mm[7]);
        CNF_ASSERT(memcmp(mm, c.mm, sizeof(mm)) == 0, c.what);
    }

    // ---- 2. total: every byte of every charset lands on an MM glyph ---------
    // (0x00-0x40, the bytes MM's printer and MM's font draw). An out-of-range
    // language (7) reads as NTSC, as SoH's own name printer reads it.
    const uint8_t langs[] = { kLangPal, kLangNtscJpn, kLangNtscEng, 7 };
    for (uint8_t lang : langs) {
        for (int b = 0; b < 256; b++) {
            const uint8_t in[8] = { (uint8_t)b, (uint8_t)b, (uint8_t)b, (uint8_t)b,
                                    (uint8_t)b, (uint8_t)b, (uint8_t)b, (uint8_t)b };
            uint8_t mm[8];
            OoT_PlayerName_ToMMCharset(in, lang, mm);
            for (int i = 0; i < 8; i++) {
                if (mm[i] > 0x40 || MMPrints(mm[i]) == '?') {
                    printf("[TEST] %s: lang %u byte 0x%02X -> 0x%02X\n", sRow, (unsigned)lang, (unsigned)b,
                           (unsigned)mm[i]);
                    CNF_ASSERT(false, "every OoT name byte lands on a byte MM can print");
                }
            }
            // ---- 3. what OoT prints, MM prints ------------------------------
            // A character OoT's printer shows (a digit, a letter, space, '-',
            // '.') prints as the same character in MM; anything else becomes
            // MM's space.
            const char want = OoTPrints((uint8_t)b, lang == kLangPal ? kLangPal : kLangNtscEng);
            const bool kana = lang == kLangNtscJpn && ((b >= 0x0A && b < 0xAB) || b == 0xE7 || b == 0xE8);
            const char expect = (want == '?' || kana) ? ' ' : want;
            if (MMPrints(mm[0]) != expect) {
                printf("[TEST] %s: lang %u byte 0x%02X: OoT prints '%c', MM prints '%c'\n", sRow, (unsigned)lang,
                       (unsigned)b, expect, MMPrints(mm[0]));
                CNF_ASSERT(false, "a character OoT prints, MM prints the same (no-glyph bytes: MM's space)");
            }
        }
    }
    // Every typable NTSC English character is covered by (3); count them so a
    // drift in either printer table cannot silently empty the check.
    int typable = 0;
    for (int b = 0; b < 256; b++) {
        typable += OoTPrints((uint8_t)b, kLangNtscEng) != '?' ? 1 : 0;
    }
    printf("[TEST] %s: NTSC English printable name bytes checked: %d (10 digits + 52 letters + space, '-', '.')\n",
           sRow, typable);
    CNF_ASSERT(typable == 65, "OoT's NTSC printer shows 65 distinct name characters");

    // ---- 4. the slot panel's decoder ----------------------------------------
    char out[9];
    RsbsSave_DecodeN64FilenameName(kCases[0].mm, out);
    printf("[TEST] %s: panel decode of MM \"Link\" = \"%s\"\n", sRow, out);
    CNF_ASSERT(strcmp(out, "Link") == 0, "the panel prints MM's \"Link\" as Link (trailing spaces dropped)");
    RsbsSave_DecodeN64FilenameName(kMMName, out);
    CNF_ASSERT(strcmp(out, kPanelName) == 0, "the panel prints the creation rows' name");
    RsbsSave_DecodeN64FilenameName(kMMDefaultName, out);
    CNF_ASSERT(out[0] == '\0', "MM's all-space default (a half created before #773) prints as no name");
    const uint8_t offCharset[8] = { 0x15, 0x41, 0xDF, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E };
    RsbsSave_DecodeN64FilenameName(offCharset, out);
    CNF_ASSERT(strcmp(out, "L??") == 0, "a byte outside MM's charset prints as '?'");

    // ---- 5. the slot panel's OoT decoder, over a synthetic OoT save ---------
    // The panel copied OoT's raw charset bytes before #773; it now prints them
    // through the same translation, in the file's own filename language.
    struct OoTPanelCase {
        uint8_t lang;
        uint8_t oot[8];
        const char* expect;
    };
    const OoTPanelCase panel[] = {
        { kLangNtscEng, { 0xAB, 0xC6, 0x01, 0xDF, 0xDE, 0xE4, 0xEA, 0xE7 }, "Ab1 z-." },
        { kLangNtscEng, { 0xB6, 0xCD, 0xD2, 0xCF, 0xDF, 0xDF, 0xDF, 0xDF }, "Link" },
        { kLangPal, { 0x15, 0x12, 0x17, 0x14, 0x3E, 0x3E, 0x3E, 0x3E }, "LINK" },
        { kLangNtscJpn, { 0x0A, 0x5A, 0xAB, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF }, "  A" },
    };
    for (const OoTPanelCase& p : panel) {
        Randomizer_TestClearOoTSave();
        Randomizer_TestSetOoTPlayerName(p.oot, p.lang);
        OoT_SlotMeta_DecodePlayerName(reinterpret_cast<const uint8_t*>(&gSaveContext), (size_t)OOT_SAVE_CONTEXT_SIZE,
                                      out);
        printf("[TEST] %s: OoT panel decode (language %u) = \"%s\"\n", sRow, (unsigned)p.lang, out);
        CNF_ASSERT(strcmp(out, p.expect) == 0, "the slot panel prints OoT's name in the file's own charset");
    }
    Randomizer_TestClearOoTSave();

    printf("[TEST] PASS: %s\n", sRow);
    return 0;
}
