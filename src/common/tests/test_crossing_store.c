/**
 * @file test_crossing_store.c
 * @brief ROM-free locks for the cross-game placement store (ADR 0010 O7,
 *        src/common/crossing_store.h) and its three routes: the .redsave v3
 *        Tier-4 block, the one spoiler's combo.crossingStore section, and the
 *        coordinator hydrate.
 *
 * Four rows, all in the display-free, ROM-free `redship` tier:
 *
 *   crossing-store-roundtrip  the WRITE path (capture from the coordinator's
 *       tables over stub engines), the READ path (both give-path accessors, the
 *       pinned table first), the HYDRATE path (store -> coordinator with no
 *       engine call, then capture again -> byte-identical), the
 *       session-invalidation KEEP/DROP rule, and a refused CAPTURE (no live
 *       pairing) leaves the store EMPTY and UNSET. Also a REGRESSION GUARD, not
 *       evidence of any code in this change: Context_FreezeState /
 *       RestoreState / ArmShadowAsFrozen leave the block byte-exact. The store
 *       lives outside every frozen blob, so that leg cannot go red for this
 *       change; it goes red only if a later change moves the store into a blob.
 *       The real switch/arrival paths are not driven here.
 *   crossing-store-capacity   exactly the cap is stored, one over is refused and
 *       never truncated (store API and a crafted .redsave alike). The .redsave
 *       leg is red-capable for the HEADER-FIRST refusal: its over-cap count has
 *       NO body behind it, so only Combo_Crossings_BlockSize's cap check yields
 *       RSBS_REFUSE_CROSSINGS (without it the reader would try the body and
 *       report RSBS_REFUSE_TRUNCATED), and BlockSize is also asserted directly.
 *       Bad rows and duplicate hosts are refused, the frozen rule refuses a
 *       different set, and a world frozen EMPTY refuses a non-empty one.
 *   crossing-store-redsave    the format version bump is asserted; save -> clear
 *       -> load is byte-identical in a ROM-free process (no MM boot, as for
 *       every redship row); the Tier-4 bytes on disk ARE the serialized block;
 *       a v2 file loads as "no crossings" and FROZEN; a CRC-clean but
 *       malformed block and a truncated block are refused; a refused load
 *       (commit skew) applies nothing.
 *   crossing-store-spoiler    store -> combo.crossingStore -> clear -> load is
 *       byte-identical; a second load is a no-op; a different frozen set, a
 *       section naming another world (or none), a world frozen with NO
 *       crossings, a digest that the rows do not reproduce, and an absent
 *       origin tag are all refused WITHOUT A WRITE (the store's write
 *       generation does not move, so publish-then-retract would be red).
 *
 * Each row owns its own save directory (rsbs_test_saves_crossings_<row>), so a
 * parallel `ctest -j` cannot race two rows on one slot file.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++)
 * for rsbs::SaveManager, like test_foreign_items.c. Every helper lives in an
 * anonymous namespace under an Xs prefix, because every test source shares one
 * translation unit.
 */

#include "../combo_logic.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../entrance.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../test_runner.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" {
int MM_Rando_WriteCrossingSpoilerSection(const char* path);
int MM_Rando_LoadCrossingsFromSpoiler(const char* path);
}

static_assert(RSBS_SAVE_VERSION == 3u && RSBS_SAVE_VERSION_CROSSINGS == 3u,
              "ADR 0010 O7 bumps the .redsave format to 3 for the Tier-4 crossing block");
static_assert(RSBS_CROSSING_STORE_CAP >= RSBS_COMBO_LOGIC_PLACEMENT_CAP,
              "the store must hold every crossing a coordinator table can carry");
static_assert(RSBS_CROSSING_RECORD_SIZE == 8u && RSBS_CROSSING_BLOCK_HEADER_SIZE == 16u,
              "the crossing block's record and header sizes are .redsave format");

#define XS_ASSERT(cond)                                                                                                \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
            XsRestore();                                                                                               \
            return TEST_FAIL;                                                                                          \
        }                                                                                                              \
    } while (0)

namespace {

// One directory PER ROW (set by XsUseDir at the top of each row): the rows
// save, load, quarantine and delete slot 0, and `ctest -j` runs them in
// parallel processes that would otherwise race on one file.
const char* sXsSaveDir = "rsbs_test_saves_crossings";

void XsUseDir(const char* dir) {
    sXsSaveDir = dir;
}

// ---- Stub engines: accept every place, count every call ------------------------

int sXsPlaceCalls = 0;
int sXsClearCalls = 0;

int XsBegin(void*) {
    return 1;
}
void XsAssume(void*, uint16_t) {
}
int XsExpand(void*) {
    return 0;
}
int XsCrossing(void*) {
    return 1;
}
int XsReached(void*, uint16_t) {
    return 1;
}
int XsHosts(void*, uint16_t*, int) {
    return 0;
}
int XsGoal(void*) {
    return 1;
}
int XsPlace(void*, uint16_t, SharedItem) {
    ++sXsPlaceCalls;
    return 1;
}
void XsClear(void*) {
    ++sXsClearCalls;
}
void XsEnd(void*) {
}

const ComboLogicEngine kXsEngine = {
    RSBS_COMBO_LOGIC_ENGINE_ABI, nullptr, XsBegin, XsAssume, XsExpand, XsCrossing, XsReached, XsHosts, XsHosts,
    XsGoal,  XsPlace, XsClear, XsEnd, nullptr, nullptr,
};

const ComboLogicEngine* sXsPriorOoT = nullptr;
const ComboLogicEngine* sXsPriorMM = nullptr;
bool sXsSwapped = false;

void XsUseStubEngines() {
    if (!sXsSwapped) {
        sXsPriorOoT = Combo_Logic_GetEngine(GAME_OOT);
        sXsPriorMM = Combo_Logic_GetEngine(GAME_MM);
        sXsSwapped = true;
    }
    Combo_Logic_RegisterEngine(GAME_OOT, &kXsEngine);
    Combo_Logic_RegisterEngine(GAME_MM, &kXsEngine);
}

/** Leave no trace for the next row (`--test all` runs every row in one process). */
void XsRestore() {
    if (sXsSwapped) {
        Combo_Logic_ResetPlacements(); // with the stubs still in: their clear is a counter
        Combo_Logic_RegisterEngine(GAME_OOT, sXsPriorOoT);
        Combo_Logic_RegisterEngine(GAME_MM, sXsPriorMM);
        sXsSwapped = false;
    }
    Combo_Crossings_Clear();
    ComboContext_Init();
    Context_ClearAllFrozenStates(); // a successful LoadSlot arms the MM half
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(sXsSaveDir);
    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    mgr.SetSaveDirectory("Save");
}

void XsPair() {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x5EED0707u;
    gComboCtx.sharedRandoSettingsHash = 0xC0FFEE07u;
}

SharedItem XsItem(GameId origin, uint16_t id) {
    SharedItem s;
    s.originGame = (uint8_t)origin;
    s.flags = 0;
    s.id = id;
    return s;
}

ComboCrossing XsRow(uint16_t host, GameId origin, uint16_t id, uint16_t itemClass) {
    ComboCrossing c;
    c.hostCheck = host;
    c.itemClass = itemClass;
    c.item = XsItem(origin, id);
    return c;
}

std::vector<uint8_t> XsBytes() {
    std::vector<uint8_t> out(Combo_Crossings_SerializedSize());
    Combo_Crossings_Serialize(out.data(), out.size());
    return out;
}

/** A known mixed world: 3 MM-origin items on OoT hosts, 4 OoT-origin on MM hosts. */
int XsReplaceKnown() {
    const ComboCrossing oot[3] = { XsRow(101, GAME_MM, 7, 0x0001), XsRow(55, GAME_MM, 300, 0x0004),
                                   XsRow(900, GAME_MM, 12, 0x0002) };
    const ComboCrossing mm[4] = { XsRow(42, GAME_OOT, 90, 0x0001), XsRow(7, GAME_OOT, 91, 0x0001),
                                  XsRow(1200, GAME_OOT, 3, 0x0010), XsRow(8, GAME_OOT, 150, 0x0001) };
    return Combo_Crossings_Replace(oot, 3, mm, 4);
}

void XsSeedShadows() {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, 0x11);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, 0x22);
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

std::vector<uint8_t> XsReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool XsWriteFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    return (bool)out;
}

/** Offset of Tier-4 in a v3 file this build wrote. */
size_t XsTier4Offset() {
    return sizeof(rsbs::RsbsSaveHeader) + RSBS_COMBO_CONTEXT_RECORD_SIZE + OOT_SAVE_CONTEXT_SIZE +
           MM_SAVE_CONTEXT_SIZE;
}

/** Re-stamp the header CRC after editing the payload, so a test reaches the
 *  check it means to reach rather than the CRC. */
void XsRestampCrc(std::vector<uint8_t>& file) {
    const uint32_t crc = rsbs::SaveManager::Crc32(file.data() + sizeof(rsbs::RsbsSaveHeader),
                                                  file.size() - sizeof(rsbs::RsbsSaveHeader));
    std::memcpy(file.data() + offsetof(rsbs::RsbsSaveHeader, crc32), &crc, sizeof(crc));
}

} // namespace

// ============================================================================
TestResult Test_CrossingStoreRoundtrip(void) {
    printf("[TEST] crossing-store-roundtrip: capture, read, hydrate, freeze/restore and invalidation (ADR 0010 O7)\n");
    XsUseDir("rsbs_test_saves_crossings_roundtrip");
    XsRestore();
    XsUseStubEngines();
    Combo_Logic_ResetPlacements();
    XsPair();

    // ---- The WRITE path: author a mixed world in the coordinator ------------
    // Own-origin rows interleaved with crossings, on both hosts; the capture
    // must keep only the crossings, in the coordinator's order.
    XS_ASSERT(Combo_Logic_Place(GAME_OOT, 10, XsItem(GAME_OOT, 1), 0x0001));  // own
    XS_ASSERT(Combo_Logic_Place(GAME_OOT, 101, XsItem(GAME_MM, 7), 0x0001));  // crossing
    XS_ASSERT(Combo_Logic_Place(GAME_MM, 42, XsItem(GAME_OOT, 90), 0x0001));  // crossing
    XS_ASSERT(Combo_Logic_Place(GAME_OOT, 11, XsItem(GAME_OOT, 2), 0x0001));  // own
    XS_ASSERT(Combo_Logic_Place(GAME_OOT, 55, XsItem(GAME_MM, 300), 0x0004)); // crossing
    XS_ASSERT(Combo_Logic_Place(GAME_MM, 3, XsItem(GAME_MM, 44), 0x0001));    // own
    XS_ASSERT(Combo_Logic_Place(GAME_MM, 7, XsItem(GAME_OOT, 91), 0x0001));   // crossing
    const int authoredPlaces = sXsPlaceCalls;

    XS_ASSERT(Combo_Crossings_CaptureFromCoordinator() == 4);
    XS_ASSERT(Combo_Crossings_Count(GAME_OOT) == 2 && Combo_Crossings_Count(GAME_MM) == 2);
    ComboCrossing c;
    XS_ASSERT(Combo_Crossings_At(GAME_OOT, 0, &c) && c.hostCheck == 101 && c.item.originGame == GAME_MM &&
              c.item.id == 7 && c.itemClass == 0x0001);
    XS_ASSERT(Combo_Crossings_At(GAME_OOT, 1, &c) && c.hostCheck == 55 && c.item.id == 300 && c.itemClass == 0x0004);
    XS_ASSERT(Combo_Crossings_At(GAME_MM, 0, &c) && c.hostCheck == 42 && c.item.originGame == GAME_OOT &&
              c.item.id == 90);
    XS_ASSERT(Combo_Crossings_At(GAME_MM, 1, &c) && c.hostCheck == 7 && c.item.id == 91);
    const std::vector<uint8_t> captured = XsBytes();
    XS_ASSERT(captured.size() == 16u + 4u * 8u);
    XS_ASSERT(std::memcmp(captured.data(), "RSXP", 4) == 0);

    // ---- The READ path: both give-path accessors, pinned first ---------------
    const SharedItem* got = Combo_GetForeignPlacementForOoTCheck(101);
    XS_ASSERT(got != nullptr && got->originGame == GAME_MM && got->id == 7);
    got = Combo_GetForeignPlacementForCheck(42);
    XS_ASSERT(got != nullptr && got->originGame == GAME_OOT && got->id == 90);
    XS_ASSERT(Combo_GetForeignPlacementForOoTCheck(10) == nullptr); // own-origin host: no crossing
    XS_ASSERT(Combo_GetForeignPlacementForCheck(3) == nullptr);
    // Direction is the accessor: MM check 101 is not OoT check 101.
    XS_ASSERT(Combo_GetForeignPlacementForCheck(101) == nullptr);
    // The pinned table keeps working, and answers first where both list a host.
    XS_ASSERT(Combo_SetForeignPlacement(42, XsItem(GAME_OOT, 5)) >= 0);
    XS_ASSERT(Combo_SetForeignPlacement(500, XsItem(GAME_OOT, 6)) >= 0);
    got = Combo_GetForeignPlacementForCheck(42);
    XS_ASSERT(got != nullptr && got->id == 5);
    got = Combo_GetForeignPlacementForCheck(500);
    XS_ASSERT(got != nullptr && got->id == 6);
    Combo_ClearForeignPlacements();
    got = Combo_GetForeignPlacementForCheck(42);
    XS_ASSERT(got != nullptr && got->id == 90);

    // ---- The HYDRATE path: store -> coordinator, no engine call ---------------
    const int placesBefore = sXsPlaceCalls;
    const int clearsBefore = sXsClearCalls;
    XS_ASSERT(Combo_Crossings_HydrateCoordinator() == 4);
    XS_ASSERT(sXsPlaceCalls == placesBefore && sXsClearCalls == clearsBefore);
    XS_ASSERT(sXsPlaceCalls == authoredPlaces);
    XS_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == 2 && Combo_Logic_PlacementCount(GAME_MM) == 2);
    ComboLogicPlacement p;
    XS_ASSERT(Combo_Logic_GetPlacement(GAME_OOT, 55, &p) && p.item.id == 300 && p.itemClass == 0x0004);
    XS_ASSERT(!Combo_Logic_GetPlacement(GAME_OOT, 10, nullptr)); // own-origin rows are not the store's
    // Round-trip: capture from the hydrated tables reproduces the bytes.
    XS_ASSERT(Combo_Crossings_CaptureFromCoordinator() == 4);
    XS_ASSERT(XsBytes() == captured);
    // Hydrate is all-or-nothing: a repeated host is refused, tables untouched.
    {
        ComboLogicPlacement dup[2];
        dup[0].hostCheck = 9;
        dup[0].itemClass = 1;
        dup[0].item = XsItem(GAME_MM, 1);
        dup[1] = dup[0];
        const uint32_t before = Combo_Logic_PlacementDigest();
        XS_ASSERT(!Combo_Logic_HydrateTables(dup, 2, nullptr, 0));
        XS_ASSERT(Combo_Logic_PlacementDigest() == before);
    }

    // ---- Freeze/restore and shadow arming never touch the block ---------------
    // A REGRESSION GUARD only: the store is outside every frozen blob, so no line
    // of this change is what keeps these bytes; the leg goes red only if a later
    // change moves the store into one. The real switch/arrival paths
    // (Combo_ConsumeFrozenState, MM_Rando_PairOnCrossGameArrival) are not driven.
    XsSeedShadows();
    {
        std::vector<uint8_t> live(MM_SAVE_CONTEXT_SIZE, 0x33);
        Context_FreezeState(GAME_MM, 0x1234, live.data(), live.size());
        XS_ASSERT(XsBytes() == captured);
        std::vector<uint8_t> back(MM_SAVE_CONTEXT_SIZE, 0);
        XS_ASSERT(Context_RestoreState(GAME_MM, back.data(), back.size()) != 0);
        XS_ASSERT(XsBytes() == captured);
        std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, 0x44);
        Context_FreezeState(GAME_OOT, 0x0042, oot.data(), oot.size());
        std::vector<uint8_t> ootBack(OOT_SAVE_CONTEXT_SIZE, 0);
        XS_ASSERT(Context_RestoreState(GAME_OOT, ootBack.data(), ootBack.size()) != 0);
        Context_UpdateShadowCopy(GAME_MM, live.data(), live.size());
        XS_ASSERT(Context_ArmShadowAsFrozen(GAME_MM, 0x0001) == 1);
        XS_ASSERT(XsBytes() == captured);
    }

    // ---- Session invalidation: KEEP on the creation path, DROP elsewhere ------
    Combo_ClearStartupEntrance();
    XsPair();
    Context_InvalidateSessionOnNewGame(1, gComboCtx.sharedRandoSeed); // this file's world: KEEP
    XS_ASSERT(XsBytes() == captured);
    Context_InvalidateSessionOnNewGame(1, 0x0BADu); // a different world: DROP
    XS_ASSERT(Combo_Crossings_Count(GAME_OOT) == 0 && Combo_Crossings_Count(GAME_MM) == 0);
    XS_ASSERT(XsReplaceKnown() == 7);
    Context_InvalidateSessionOnSlotLoad();
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
    XS_ASSERT(XsReplaceKnown() == 7);
    XS_ASSERT(Context_InvalidateSessionOnReturnToTitle() == 1);
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);

    // ---- A refused capture leaves the store EMPTY ------------------------------
    XS_ASSERT(XsReplaceKnown() == 7);
    gComboCtx.sourceIsRando = false; // no live pairing
    XS_ASSERT(Combo_Crossings_CaptureFromCoordinator() == RSBS_CROSSING_ERR_NOT_PAIRED);
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
    XS_ASSERT(!Combo_Crossings_IsFrozen()); // a refused creation leaves nothing frozen
    XS_ASSERT(Combo_GetForeignPlacementForOoTCheck(101) == nullptr);

    XsRestore();
    printf("[TEST] PASS: capture keeps crossings only, both accessors read them (pinned first), hydrate calls no "
           "engine and round-trips byte-identical, freeze/restore/arm leave the block exact, KEEP/DROP hold, a "
           "refused capture leaves the store empty\n");
    return TEST_PASS;
}

// ============================================================================
TestResult Test_CrossingStoreCapacity(void) {
    printf("[TEST] crossing-store-capacity: the cap is stored, one over is refused, never truncated (ADR 0010 O7)\n");
    XsUseDir("rsbs_test_saves_crossings_capacity");
    XsRestore();
    const int cap = (int)RSBS_CROSSING_STORE_CAP;

    // The HEADER-FIRST bound, directly: Combo_Crossings_BlockSize is what the
    // .redsave reader sizes the body read from, and what CrossingParse relies on
    // before filling a cap-sized scratch table. Exactly the cap sizes; one over,
    // on either side, is CAPACITY from the 16 header bytes alone.
    {
        uint8_t hdr[RSBS_CROSSING_BLOCK_HEADER_SIZE] = { 'R', 'S', 'X', 'P', 1, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        const uint16_t atCapCount = (uint16_t)cap;
        const uint16_t overCount = (uint16_t)(cap + 1);
        size_t total = 0;
        hdr[8] = (uint8_t)(atCapCount & 0xFF);
        hdr[9] = (uint8_t)(atCapCount >> 8);
        hdr[10] = (uint8_t)(atCapCount & 0xFF);
        hdr[11] = (uint8_t)(atCapCount >> 8);
        XS_ASSERT(Combo_Crossings_BlockSize(hdr, sizeof(hdr), &total) == RSBS_CROSSING_OK);
        XS_ASSERT(total == RSBS_CROSSING_BLOCK_MAX_SIZE);
        hdr[8] = (uint8_t)(overCount & 0xFF);
        hdr[9] = (uint8_t)(overCount >> 8);
        total = 12345;
        XS_ASSERT(Combo_Crossings_BlockSize(hdr, sizeof(hdr), &total) == RSBS_CROSSING_ERR_CAPACITY);
        XS_ASSERT(total == 12345); // nothing sized from a refused header
        hdr[8] = 0;
        hdr[9] = 0;
        hdr[10] = (uint8_t)(overCount & 0xFF);
        hdr[11] = (uint8_t)(overCount >> 8);
        XS_ASSERT(Combo_Crossings_BlockSize(hdr, sizeof(hdr), &total) == RSBS_CROSSING_ERR_CAPACITY);
        hdr[10] = 0xFF; // the largest count a u16 can claim
        hdr[11] = 0xFF;
        XS_ASSERT(Combo_Crossings_BlockSize(hdr, sizeof(hdr), &total) == RSBS_CROSSING_ERR_CAPACITY);
    }
    std::vector<ComboCrossing> oot((size_t)cap + 1);
    std::vector<ComboCrossing> mm((size_t)cap + 1);
    for (int i = 0; i <= cap; ++i) {
        oot[(size_t)i] = XsRow((uint16_t)(i + 1), GAME_MM, (uint16_t)(1000 + i), 0x0001);
        mm[(size_t)i] = XsRow((uint16_t)(i + 1), GAME_OOT, (uint16_t)(2000 + i), 0x0001);
    }

    // One over, into an EMPTY store: refused, nothing stored.
    XS_ASSERT(Combo_Crossings_Replace(oot.data(), cap + 1, mm.data(), 0) == RSBS_CROSSING_ERR_CAPACITY);
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
    XS_ASSERT(Combo_Crossings_Replace(nullptr, 0, mm.data(), cap + 1) == RSBS_CROSSING_ERR_CAPACITY);
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);

    // Exactly the cap, both directions: stored whole.
    XS_ASSERT(Combo_Crossings_Replace(oot.data(), cap, mm.data(), cap) == 2 * cap);
    XS_ASSERT(Combo_Crossings_Count(GAME_OOT) == cap && Combo_Crossings_Count(GAME_MM) == cap);
    XS_ASSERT(Combo_Crossings_SerializedSize() == RSBS_CROSSING_BLOCK_MAX_SIZE);
    const std::vector<uint8_t> atCap = XsBytes();
    ComboCrossing last;
    XS_ASSERT(Combo_Crossings_At(GAME_MM, cap - 1, &last) && last.item.id == (uint16_t)(2000 + cap - 1));

    // One over, into a FULL store: refused and the resident set untouched.
    XS_ASSERT(Combo_Crossings_Replace(oot.data(), cap + 1, mm.data(), cap) == RSBS_CROSSING_ERR_CAPACITY);
    XS_ASSERT(XsBytes() == atCap);

    // The .redsave at the cap: saved and loaded whole.
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(sXsSaveDir);
    XsSeedShadows();
    ComboContext_Init();
    mgr.DeleteSave(0);
    XS_ASSERT(mgr.Save(0));
    Combo_Crossings_Clear();
    XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK);
    XS_ASSERT(XsBytes() == atCap);

    // A .redsave whose block claims cap + 1 on one side: refused from the block
    // header, before the body is read. The body is deliberately SHORT for the
    // claimed count (no bytes for the extra row), so the reason tells which
    // check fired: the header-first cap refusal is RSBS_REFUSE_CROSSINGS, while
    // a reader that skipped it would try to read the body and report
    // RSBS_REFUSE_TRUNCATED. The store keeps what it held; the evidence is
    // quarantined.
    {
        std::vector<uint8_t> file = XsReadFile(mgr.SlotPath(0));
        const size_t t4 = XsTier4Offset();
        XS_ASSERT(file.size() == t4 + RSBS_CROSSING_BLOCK_MAX_SIZE);
        const uint16_t over = (uint16_t)(cap + 1);
        file[t4 + 8] = (uint8_t)(over & 0xFF);
        file[t4 + 9] = (uint8_t)(over >> 8);
        XsRestampCrc(file);
        XS_ASSERT(XsWriteFile(mgr.SlotPath(0), file));
        Combo_Crossings_Clear();
        XS_ASSERT(XsReplaceKnown() == 7);
        const std::vector<uint8_t> resident = XsBytes();
        XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED);
        XS_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_CROSSINGS);
        XS_ASSERT(XsBytes() == resident);
        XS_ASSERT(mgr.HasQuarantine(0));
        mgr.DeleteSave(0);
    }

    // Bad rows: host 0, an own-origin row, an untagged row, a repeated host.
    Combo_Crossings_Clear();
    {
        const ComboCrossing host0[1] = { XsRow(0, GAME_MM, 1, 1) };
        XS_ASSERT(Combo_Crossings_Replace(host0, 1, nullptr, 0) == RSBS_CROSSING_ERR_BAD_ROW);
        const ComboCrossing own[1] = { XsRow(5, GAME_OOT, 1, 1) };
        XS_ASSERT(Combo_Crossings_Replace(own, 1, nullptr, 0) == RSBS_CROSSING_ERR_BAD_ROW);
        const ComboCrossing untagged[1] = { XsRow(5, GAME_NONE, 1, 1) };
        XS_ASSERT(Combo_Crossings_Replace(nullptr, 0, untagged, 1) == RSBS_CROSSING_ERR_BAD_ROW);
        const ComboCrossing dup[2] = { XsRow(5, GAME_OOT, 1, 1), XsRow(5, GAME_OOT, 2, 1) };
        XS_ASSERT(Combo_Crossings_Replace(nullptr, 0, dup, 2) == RSBS_CROSSING_ERR_DUPLICATE_HOST);
        // One host in EACH game is two different hosts, not a duplicate.
        const ComboCrossing o[1] = { XsRow(5, GAME_MM, 1, 1) };
        const ComboCrossing m[1] = { XsRow(5, GAME_OOT, 1, 1) };
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
        XS_ASSERT(Combo_Crossings_Replace(o, 1, m, 1) == 2);
    }

    // The frozen rule: an identical set is a no-op, a different one is refused.
    Combo_Crossings_Clear();
    XS_ASSERT(XsReplaceKnown() == 7);
    const std::vector<uint8_t> frozen = XsBytes();
    XS_ASSERT(XsReplaceKnown() == 7);
    XS_ASSERT(XsBytes() == frozen);
    {
        const ComboCrossing other[1] = { XsRow(101, GAME_MM, 8, 0x0001) };
        XS_ASSERT(Combo_Crossings_Replace(other, 1, nullptr, 0) == RSBS_CROSSING_ERR_DIVERGED);
        XS_ASSERT(Combo_Crossings_Replace(nullptr, 0, nullptr, 0) == RSBS_CROSSING_ERR_DIVERGED);
        XS_ASSERT(XsBytes() == frozen);
    }

    // FROZEN EMPTY is not UNSET: a v3 block with both counts 0 freezes a world
    // with no crossings, and a non-empty set is then divergence, not a fill-in.
    Combo_Crossings_Clear();
    XS_ASSERT(!Combo_Crossings_IsFrozen());
    {
        const std::vector<uint8_t> emptyBlock = XsBytes();
        XS_ASSERT(emptyBlock.size() == 16u);
        XS_ASSERT(Combo_Crossings_LoadBlock(emptyBlock.data(), emptyBlock.size()) == RSBS_CROSSING_OK);
        XS_ASSERT(Combo_Crossings_IsFrozen());
        const uint32_t gen = Combo_Crossings_WriteGeneration();
        XS_ASSERT(XsReplaceKnown() == RSBS_CROSSING_ERR_DIVERGED);
        XS_ASSERT(Combo_Crossings_WriteGeneration() == gen);
        XS_ASSERT(XsBytes() == emptyBlock);
        XS_ASSERT(Combo_Crossings_Replace(nullptr, 0, nullptr, 0) == 0); // agreeing with it is a no-op
        XS_ASSERT(Combo_Crossings_WriteGeneration() == gen);
    }

    XsRestore();
    printf("[TEST] PASS: %d per host stored whole, %d refused and never truncated (API and .redsave), bad rows "
           "refused, frozen set refuses divergence\n",
           cap, cap + 1);
    return TEST_PASS;
}

// ============================================================================
TestResult Test_CrossingStoreRedsave(void) {
    printf("[TEST] crossing-store-redsave: format v3 Tier-4 round-trip, legacy, malformed, refused (ADR 0010 O7)\n");
    XsUseDir("rsbs_test_saves_crossings_redsave");
    XsRestore();
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(sXsSaveDir);
    XsSeedShadows();
    ComboContext_Init();
    mgr.DeleteSave(0);

    XS_ASSERT(XsReplaceKnown() == 7);
    const std::vector<uint8_t> known = XsBytes();
    XS_ASSERT(mgr.Save(0));

    // The version bump, on disk.
    std::vector<uint8_t> file = XsReadFile(mgr.SlotPath(0));
    rsbs::RsbsSaveHeader h;
    XS_ASSERT(file.size() >= sizeof(h));
    std::memcpy(&h, file.data(), sizeof(h));
    XS_ASSERT(h.version == 3u && h.version == RSBS_SAVE_VERSION_CROSSINGS);
    // Tier-4 on disk IS the serialized block, right after Tier-3.
    const size_t t4 = XsTier4Offset();
    XS_ASSERT(file.size() == t4 + known.size());
    XS_ASSERT(std::equal(known.begin(), known.end(), file.begin() + (std::ptrdiff_t)t4));

    // save -> clear -> load: byte-identical, in this ROM-free process (MM is not
    // booted, as in every redship row; the load needs no MM code).
    Context_InvalidateSessionOnSlotLoad();
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
    XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK);
    XS_ASSERT(XsBytes() == known);
    const SharedItem* got = Combo_GetForeignPlacementForCheck(1200);
    XS_ASSERT(got != nullptr && got->originGame == GAME_OOT && got->id == 3);

    // A REFUSED load applies nothing: OoT's .sav claims a newer commit than
    // this file (commit skew), so the load refuses before committing.
    Context_InvalidateSessionOnSlotLoad();
    XS_ASSERT(mgr.LoadSlot(0, 0x7FFFFFFFu) == RSBS_LOAD_REFUSED);
    XS_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_COMMIT_SKEW);
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
    mgr.DeleteSave(0);

    // A CRC-clean block no writer could produce (an own-origin row) is refused.
    {
        std::vector<uint8_t> bad = file;
        bad[t4 + 16 + 4] = (uint8_t)GAME_OOT; // first OoT-hosted row now claims OoT origin
        XsRestampCrc(bad);
        XS_ASSERT(XsWriteFile(mgr.SlotPath(0), bad));
        XS_ASSERT(XsReplaceKnown() == 7);
        XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED);
        XS_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_CROSSINGS);
        XS_ASSERT(XsBytes() == known);
        mgr.DeleteSave(0);
    }
    // A bad block magic is refused too.
    {
        std::vector<uint8_t> bad = file;
        bad[t4] = 'X';
        XsRestampCrc(bad);
        XS_ASSERT(XsWriteFile(mgr.SlotPath(0), bad));
        XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED);
        XS_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_CROSSINGS);
        mgr.DeleteSave(0);
    }
    // A truncated block is TRUNCATED, not a shorter crossing set.
    {
        std::vector<uint8_t> bad(file.begin(), file.end() - 4);
        XS_ASSERT(XsWriteFile(mgr.SlotPath(0), bad));
        XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED);
        XS_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_TRUNCATED);
        XS_ASSERT(XsBytes() == known);
        mgr.DeleteSave(0);
    }
    // A v2 file (Tiers 1-3, no block) loads as "no crossings".
    {
        std::vector<uint8_t> v2(file.begin(), file.begin() + (std::ptrdiff_t)t4);
        rsbs::RsbsSaveHeader h2;
        std::memcpy(&h2, v2.data(), sizeof(h2));
        h2.version = RSBS_SAVE_VERSION_CROSSINGS - 1u;
        std::memcpy(v2.data(), &h2, sizeof(h2));
        XsRestampCrc(v2);
        XS_ASSERT(XsWriteFile(mgr.SlotPath(0), v2));
        XS_ASSERT(Combo_Crossings_SerializedSize() > 16u);
        XS_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK);
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
        // ...and FROZEN empty: no pre-crossing build could give that world a
        // crossing, so a later set of rows is divergence, never a fill-in.
        XS_ASSERT(Combo_Crossings_IsFrozen());
        const uint32_t gen = Combo_Crossings_WriteGeneration();
        XS_ASSERT(XsReplaceKnown() == RSBS_CROSSING_ERR_DIVERGED);
        XS_ASSERT(Combo_Crossings_WriteGeneration() == gen);
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
        mgr.DeleteSave(0);
    }

    XsRestore();
    printf("[TEST] PASS: v3 on disk, Tier-4 == the serialized block, load byte-identical with MM unbooted, refused "
           "load applies nothing, malformed/truncated refused, v2 loads empty\n");
    return TEST_PASS;
}

// ============================================================================
TestResult Test_CrossingStoreSpoiler(void) {
    printf("[TEST] crossing-store-spoiler: combo.crossingStore write -> load byte-identical (ADR 0010 O7)\n");
    XsUseDir("rsbs_test_saves_crossings_spoiler");
    XsRestore();
    XsPair(); // the section is written for, and loaded into, THIS world (#610)
    std::error_code ec;
    std::filesystem::create_directories(sXsSaveDir, ec);
    const std::string path = std::string(sXsSaveDir) + "/xs-spoiler.json";
    std::filesystem::remove(path, ec);

    // An existing OoT-shaped document keeps its other keys.
    XS_ASSERT(XsWriteFile(path, std::vector<uint8_t>({ '{', '"', 's', 'e', 'e', 'd', '"', ':', '1', '}' })));
    XS_ASSERT(XsReplaceKnown() == 7);
    const std::vector<uint8_t> known = XsBytes();
    XS_ASSERT(MM_Rando_WriteCrossingSpoilerSection(path.c_str()) == 0);
    std::string text;
    {
        const std::vector<uint8_t> raw = XsReadFile(path);
        text.assign(raw.begin(), raw.end());
    }
    XS_ASSERT(text.find("\"seed\"") != std::string::npos);
    XS_ASSERT(text.find("\"crossingStore\"") != std::string::npos);
    XS_ASSERT(text.find("\"identity\"") != std::string::npos);
    char digest[16];
    snprintf(digest, sizeof(digest), "%08X", (unsigned)Combo_Crossings_Digest());
    XS_ASSERT(text.find(digest) != std::string::npos);

    // write -> clear -> load: byte-identical.
    Combo_Crossings_Clear();
    XS_ASSERT(MM_Rando_LoadCrossingsFromSpoiler(path.c_str()) == 7);
    XS_ASSERT(XsBytes() == known);
    // A second load of the same world is a no-op.
    XS_ASSERT(MM_Rando_LoadCrossingsFromSpoiler(path.c_str()) == 7);
    XS_ASSERT(XsBytes() == known);

    // A DIFFERENT frozen set is resident: the spoiler is refused, nothing moves.
    Combo_Crossings_Clear();
    {
        const ComboCrossing other[1] = { XsRow(3, GAME_MM, 3, 0x0001) };
        XS_ASSERT(Combo_Crossings_Replace(other, 1, nullptr, 0) == 1);
        const std::vector<uint8_t> resident = XsBytes();
        XS_ASSERT(MM_Rando_LoadCrossingsFromSpoiler(path.c_str()) < 0);
        XS_ASSERT(XsBytes() == resident);
    }

    // Every refusal below starts from an UNSET store and must not WRITE to it:
    // the write generation is sampled around the call, so a loader that
    // published the rows and then retracted them (same end state) is red.
    const auto refusedWithoutWrite = [&](const std::string& body, int expected) {
        XsWriteFile(path, std::vector<uint8_t>(body.begin(), body.end()));
        const uint32_t gen = Combo_Crossings_WriteGeneration();
        const int rc = MM_Rando_LoadCrossingsFromSpoiler(path.c_str());
        if (rc != expected) {
            printf("[TEST] crossing-store-spoiler: load returned %d, expected %d\n", rc, expected);
            return false;
        }
        if (Combo_Crossings_WriteGeneration() != gen) {
            printf("[TEST] crossing-store-spoiler: a refused load (%d) WROTE to the store (generation %u -> %u)\n", rc,
                   (unsigned)gen, (unsigned)Combo_Crossings_WriteGeneration());
            return false;
        }
        return true;
    };

    // The #610 rule: the section loads only into the world it names.
    Combo_Crossings_Clear();
    {
        gComboCtx.sharedRandoSeed ^= 0x00BADBADu; // another world's seed
        const bool refused = refusedWithoutWrite(text, -8);
        gComboCtx.sharedRandoSeed ^= 0x00BADBADu;
        XS_ASSERT(refused);
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u && !Combo_Crossings_IsFrozen());

        gComboCtx.sharedRandoSettingsHash ^= 0x1u; // another settings profile
        const bool refusedSettings = refusedWithoutWrite(text, -8);
        gComboCtx.sharedRandoSettingsHash ^= 0x1u;
        XS_ASSERT(refusedSettings);

        gComboCtx.sourceIsRando = false; // no live pairing at all
        const bool refusedUnpaired = refusedWithoutWrite(text, -8);
        gComboCtx.sourceIsRando = true;
        XS_ASSERT(refusedUnpaired);

        std::string anonymous = text; // a section that names no world
        const size_t at = anonymous.find("\"identity\"");
        XS_ASSERT(at != std::string::npos);
        anonymous.replace(at, std::strlen("\"identity\""), "\"identit_\"");
        XS_ASSERT(refusedWithoutWrite(anonymous, -8));
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u && !Combo_Crossings_IsFrozen());
    }

    // A world frozen with NO crossings (a v1/v2 .redsave load) refuses the rows.
    Combo_Crossings_Clear();
    XS_ASSERT(Combo_Crossings_LoadBlock(nullptr, 0) == RSBS_CROSSING_OK);
    XS_ASSERT(Combo_Crossings_IsFrozen());
    XS_ASSERT(refusedWithoutWrite(text, -6));
    XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);

    // Rows that do not reproduce the printed digest (an edited item id).
    Combo_Crossings_Clear();
    {
        std::string edited = text;
        const size_t at = edited.find("\"itemId\": 7,");
        XS_ASSERT(at != std::string::npos);
        edited.replace(at, std::strlen("\"itemId\": 7,"), "\"itemId\": 8,");
        XS_ASSERT(refusedWithoutWrite(edited, -7));
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u && !Combo_Crossings_IsFrozen());
    }
    // An absent origin tag refuses the row, and the row refuses the section.
    {
        std::string edited = text;
        const size_t at = edited.find("\"origin\":");
        XS_ASSERT(at != std::string::npos);
        edited.replace(at, std::strlen("\"origin\":"), "\"orig1n\":");
        XS_ASSERT(refusedWithoutWrite(edited, -5));
        XS_ASSERT(Combo_Crossings_SerializedSize() == 16u);
    }
    // Non-vacuity: the untouched section still loads into this world.
    XS_ASSERT(XsWriteFile(path, std::vector<uint8_t>(text.begin(), text.end())));
    XS_ASSERT(MM_Rando_LoadCrossingsFromSpoiler(path.c_str()) == 7);
    XS_ASSERT(XsBytes() == known);

    std::filesystem::remove(path, ec);
    XsRestore();
    printf("[TEST] PASS: the spoiler section round-trips byte-identical, reloads as a no-op, and refuses a different "
           "frozen set, another world's identity, a world frozen empty, an unreproduced digest and an absent origin "
           "without a write to the store\n");
    return TEST_PASS;
}

#undef XS_ASSERT
