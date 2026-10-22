/**
 * @file test_playtime_frozen.c
 * @brief MmGossipStoneSeed and MmTrapRollFrozen: what MM's randomizer decides
 *        while you play is a pure function of the frozen world (#1047).
 *
 * ROM-free, redship tier. Each row prints its results as "[pure] " lines, and a
 * two-process row (CMake/CheckPureTwoRuns.cmake) runs it twice and requires
 * the two processes to print the same lines.
 *
 * mm-gossip-stone-seed: a free gossip stone's seed (EnGs.cpp GossipStoneSeed)
 *   1. MM's stone table (every En_Gs placement in the scene actor lists) is not
 *      empty and holds stones whose position sum is negative, the inputs the
 *      float-to-uint32_t conversion left undefined;
 *   2. every stone's seed is (uint32_t)(sceneId + x + z) in integer arithmetic;
 *   3. what every free stone says (MM_Rando_Hints_DigestFreeStones, the content
 *      digest's mmFreeStoneDigest) does not depend on where MM's global stream
 *      was, and does depend on the world's seed.
 *
 * mm-trap-roll-frozen: a trap's type (Traps.cpp RollTrapType)
 *   1. the same world seed, check and frozen set give the same type whatever
 *      MM's global stream held before the roll;
 *   2. the live gRando.Traps.* CVars are ignored: with every one on and the
 *      frozen set Freeze only, every roll is Freeze;
 *   3. every roll is in the frozen set, and the full set springs more than one
 *      type over the checks (the roll is not stuck on one type);
 *   4. an empty set (every save from before #1047) springs Freeze only;
 *   5. creation (Rando::Foreign::ResolvePairedProfile) freezes the set the
 *      CVars ask for into the save;
 *   6. the trap-type rows author that set (#1101 lane R4): through the gated
 *      writer the MM Randomizer page's seven rows use, each type alone is
 *      the set creation freezes and gives the profile digest a value of its
 *      own; a frozen profile refuses the write; Reset clears the set.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++).
 */

#include "../combo_mm_options_page.h"
#include "../context.h"
#include "../game.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
// EnGs.cpp (#1047)
int MM_Rando_Hints_TestGossipStone(int index, int16_t* outSceneId, int16_t* outX, int16_t* outZ, uint32_t* outSeed);
int MM_Rando_Hints_TestFreeStonesAfterStream(uint32_t finalSeed, uint64_t streamSeed, uint32_t* outDigest);
void MM_Rando_Hints_TestSetCheck(uint16_t randoCheckId, uint16_t randoItemId, int shuffled, int obtained,
                                 uint16_t* priorItemId, int* priorShuffled, int* priorObtained);
int MM_HintArea_TestCheckRow(uint16_t id, int* outIsEnemyDrop);
int MM_HintArea_TestCheckIdMax(void);
// Traps.cpp (#1047)
int MM_Rando_Traps_TestRoll(uint32_t finalSeed, uint8_t mask, uint16_t randoCheckId, uint64_t streamSeed);
const char* MM_Rando_Traps_TestCVarName(int type);
int MM_Rando_Traps_TestTypeCount(void);
int MM_Rando_Traps_TestCreationFreezes(void);
}

namespace {

const uint32_t kPfWorldSeeds[] = { 0x74C32F12u, 0xE74E56F7u, 0x00000001u };

struct PfPrior {
    uint16_t id;
    uint16_t item;
    int shuffled;
    int obtained;
};

} // namespace

TestResult Test_MmGossipStoneSeed(void) {
    printf("[TEST] mm-gossip-stone-seed: a free gossip stone's seed and what it says are pure functions of the "
           "frozen world (#1047)\n");
    int16_t sceneId = 0;
    int16_t x = 0;
    int16_t z = 0;
    uint32_t seed = 0;
    const int count = MM_Rando_Hints_TestGossipStone(-1, &sceneId, &x, &z, &seed);
    if (count <= 0) {
        printf("[TEST] FAIL: MM's gossip stone table is empty\n");
        return TEST_FAIL;
    }
    int negative = 0;
    int wrong = 0;
    for (int i = 0; i < count; i++) {
        MM_Rando_Hints_TestGossipStone(i, &sceneId, &x, &z, &seed);
        const int32_t sum = (int32_t)sceneId + (int32_t)x + (int32_t)z;
        const uint32_t expected = (uint32_t)sum;
        negative += sum < 0 ? 1 : 0;
        printf("[pure] stone %d scene=%d x=%d z=%d seed=%08X\n", i, (int)sceneId, (int)x, (int)z, (unsigned)seed);
        fflush(stdout); // whole line before any log write interleaves (#1107 CI)
        if (seed != expected) {
            printf("[TEST]   stone %d (scene %d, x %d, z %d): seed %08X, expected %08X\n", i, (int)sceneId, (int)x,
                   (int)z, (unsigned)seed, (unsigned)expected);
            wrong++;
        }
    }
    printf("[TEST]   %d stones, %d with a negative position sum\n", count, negative);
    if (negative == 0) {
        printf("[TEST] FAIL: no stone has a negative position sum, so the row proves nothing about the conversion\n");
        return TEST_FAIL;
    }
    if (wrong != 0) {
        printf("[TEST] FAIL: %d stone seeds are not the integer sum of the stone's scene and position\n", wrong);
        return TEST_FAIL;
    }

    // A small world for the stones to hint: the first eight checks that have an
    // area hold the Lens of Truth, shuffled and not obtained.
    SharedItem mmLens;
    if (!TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmLens)) {
        printf("[TEST] FAIL: MM's Lens of Truth could not be found\n");
        return TEST_FAIL;
    }
    std::vector<PfPrior> priors;
    const int idMax = MM_HintArea_TestCheckIdMax();
    for (int id = 1; id < idMax && priors.size() < 8; id++) {
        if (!MM_HintArea_TestCheckRow((uint16_t)id, NULL)) {
            continue;
        }
        PfPrior prior = { (uint16_t)id, 0, 0, 0 };
        MM_Rando_Hints_TestSetCheck((uint16_t)id, mmLens.id, 1, 0, &prior.item, &prior.shuffled, &prior.obtained);
        priors.push_back(prior);
    }
    bool ok = priors.size() == 8;
    if (!ok) {
        printf("[TEST] FAIL: fewer than eight MM checks have an area\n");
    }
    std::set<uint32_t> perWorld;
    for (const uint32_t worldSeed : kPfWorldSeeds) {
        if (!ok) {
            break;
        }
        uint32_t first = 0;
        uint32_t second = 0;
        MM_Rando_Hints_TestFreeStonesAfterStream(worldSeed, 0x1234ull, &first);
        MM_Rando_Hints_TestFreeStonesAfterStream(worldSeed, 0xFEDCBA9876543210ull, &second);
        printf("[pure] world %08X freeStoneDigest=%08X\n", (unsigned)worldSeed, (unsigned)first);
        fflush(stdout); // whole line before any log write interleaves (#1107 CI)
        if (first != second) {
            printf("[TEST] FAIL: world %08X: what the free stones say depends on where MM's stream was (%08X / "
                   "%08X)\n",
                   (unsigned)worldSeed, (unsigned)first, (unsigned)second);
            ok = false;
        }
        perWorld.insert(first);
    }
    for (const PfPrior& prior : priors) {
        MM_Rando_Hints_TestSetCheck(prior.id, prior.item, prior.shuffled, prior.obtained, NULL, NULL, NULL);
    }
    if (ok && perWorld.size() < 2) {
        printf("[TEST] FAIL: the free stones say the same in every world, so the digest does not read the world's "
               "seed\n");
        ok = false;
    }
    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-gossip-stone-seed\n");
    return TEST_PASS;
}

// The row's body. Test_MmTrapRollFrozen (test_runner.cpp) brings up the
// display-free Ship::Context first: the CVars live on it.
static TestResult MmTrapRollFrozen_Run(void) {
    printf("[TEST] mm-trap-roll-frozen: a trap's type is a pure function of the frozen world and the check "
           "(#1047)\n");
    const int typeCount = MM_Rando_Traps_TestTypeCount();
    if (typeCount <= 1 || typeCount > 8) {
        printf("[TEST] FAIL: %d trap types\n", typeCount);
        return TEST_FAIL;
    }
    const uint8_t fullMask = (uint8_t)((1u << typeCount) - 1u);
    const uint8_t freezeOnly = 1u; // TRAP_FREEZE is type 0
    const int idMax = MM_HintArea_TestCheckIdMax();

    // Every live CVar on, for the whole row: none of it may reach a roll.
    for (int t = 0; t < typeCount; t++) {
        CVarSetInteger(MM_Rando_Traps_TestCVarName(t), 1);
    }

    bool ok = true;
    int streamMoved = 0;
    int cvarLeaks = 0;
    int outOfSet = 0;
    int notFreeze = 0;
    std::set<int> fullTypes;
    for (const uint32_t worldSeed : kPfWorldSeeds) {
        std::string line;
        for (int id = 1; id < idMax; id++) {
            const int a = MM_Rando_Traps_TestRoll(worldSeed, fullMask, (uint16_t)id, 0x1234ull);
            const int b =
                MM_Rando_Traps_TestRoll(worldSeed, fullMask, (uint16_t)id, (uint64_t)id * 0x9E3779B97F4A7C15ull);
            streamMoved += a != b ? 1 : 0;
            outOfSet += (a < 0 || a >= typeCount) ? 1 : 0;
            fullTypes.insert(a);
            line += (char)('0' + a);
            const int odd = 0x55 & fullMask;
            const int c = MM_Rando_Traps_TestRoll(worldSeed, (uint8_t)odd, (uint16_t)id, 0x77ull);
            outOfSet += (c < 0 || c >= typeCount || !(odd & (1 << c))) ? 1 : 0;
            cvarLeaks += MM_Rando_Traps_TestRoll(worldSeed, freezeOnly, (uint16_t)id, 0x99ull) != 0 ? 1 : 0;
            notFreeze += MM_Rando_Traps_TestRoll(worldSeed, 0, (uint16_t)id, 0xABull) != 0 ? 1 : 0;
        }
        printf("[pure] world %08X traps=%s\n", (unsigned)worldSeed, line.c_str());
        fflush(stdout); // whole line before any log write interleaves (#1107 CI)
    }
    printf("[TEST]   %d checks x %zu worlds: %zu distinct types under the full set\n", idMax - 1,
           sizeof(kPfWorldSeeds) / sizeof(kPfWorldSeeds[0]), fullTypes.size());
    if (streamMoved != 0) {
        printf("[TEST] FAIL: %d rolls changed with where MM's stream was before the roll\n", streamMoved);
        ok = false;
    }
    if (cvarLeaks != 0) {
        printf("[TEST] FAIL: %d rolls under a Freeze-only frozen set were not Freeze (the live CVars reached them)\n",
               cvarLeaks);
        ok = false;
    }
    if (outOfSet != 0) {
        printf("[TEST] FAIL: %d rolls fell outside the frozen set\n", outOfSet);
        ok = false;
    }
    if (fullTypes.size() < 2) {
        printf("[TEST] FAIL: the full set springs only one type\n");
        ok = false;
    }
    if (notFreeze != 0) {
        printf("[TEST] FAIL: %d rolls under an empty set (a save from before #1047) were not Freeze\n", notFreeze);
        ok = false;
    }

    // Creation freezes what the CVars ask for: every type on, then Blast alone.
    const int frozenAll = MM_Rando_Traps_TestCreationFreezes();
    for (int t = 0; t < typeCount; t++) {
        CVarClear(MM_Rando_Traps_TestCVarName(t));
    }
    CVarSetInteger(MM_Rando_Traps_TestCVarName(1), 1);
    const int frozenOne = MM_Rando_Traps_TestCreationFreezes();
    CVarClear(MM_Rando_Traps_TestCVarName(1));
    const int frozenNone = MM_Rando_Traps_TestCreationFreezes();
    printf("[pure] creation froze %02X / %02X / %02X\n", (unsigned)frozenAll, (unsigned)frozenOne,
           (unsigned)frozenNone);
    fflush(stdout); // whole line before any log write interleaves (#1107 CI)
    if (frozenAll != fullMask || frozenOne != 0x02 || frozenNone != 0) {
        printf("[TEST] FAIL: creation froze %02X, %02X and %02X; expected %02X (every CVar on), 02 (Blast) and 00 "
               "(none)\n",
               (unsigned)frozenAll, (unsigned)frozenOne, (unsigned)frozenNone, (unsigned)fullMask);
        ok = false;
    }

    // 6. The trap-type rows author the set (#1101 lane R4), through the writer
    // the rows call. Unfrozen for the leg; the stamp is put back after it.
    Combo_MMOptionsPages_Init();
    const uint32_t priorDigest = gComboCtx.mmProfileDigest;
    gComboCtx.mmProfileDigest = 0;
    if (Combo_MMTrapTypeCount() != typeCount) {
        printf("[TEST] FAIL: the trap-type rows' table holds %d types, MM has %d\n", Combo_MMTrapTypeCount(),
               typeCount);
        ok = false;
    } else {
        auto clearAll = [typeCount]() {
            for (int u = 0; u < typeCount; u++) {
                Combo_MMTrapTypeClear(Combo_MMTrapTypeAt(u));
            }
        };
        clearAll();
        const uint32_t noneStamp = MM_Rando_ComputeProfileStamp();
        std::set<uint32_t> stamps = { noneStamp };
        int followed = 0;
        for (int t = 0; t < typeCount; t++) {
            const ComboMMTrapTypeDesc* d = Combo_MMTrapTypeAt(t);
            const bool row = d != nullptr && d->type == (uint8_t)t && d->cvar != nullptr &&
                             MM_Rando_Traps_TestCVarName(t) != nullptr &&
                             strcmp(d->cvar, MM_Rando_Traps_TestCVarName(t)) == 0;
            clearAll();
            const bool wrote = Combo_MMTrapTypeSetValue(d, true);
            const int frozenSet = MM_Rando_Traps_TestCreationFreezes();
            const uint32_t stamp = MM_Rando_ComputeProfileStamp();
            const bool own = stamps.insert(stamp).second;
            if (!row || !wrote || frozenSet != (1 << t) || !own) {
                printf("[TEST] FAIL: trap row %d (%s): row %d, written %d, creation froze %02X (expected %02X), "
                       "digest %08X %s\n",
                       t, d != nullptr && d->label != nullptr ? d->label : "(none)", row ? 1 : 0, wrote ? 1 : 0,
                       (unsigned)frozenSet, (unsigned)(1u << t), (unsigned)stamp,
                       own ? "of its own" : "shared with another set");
                ok = false;
            } else {
                followed++;
            }
        }
        // Frozen: the last type stays on, and neither another type nor a clear lands.
        const int last = typeCount - 1;
        gComboCtx.mmProfileDigest = 0x4D4D0047u;
        const bool setRefused = !Combo_MMTrapTypeSetValue(Combo_MMTrapTypeAt(0), true);
        Combo_MMTrapTypeClear(Combo_MMTrapTypeAt(last));
        gComboCtx.mmProfileDigest = 0;
        const int frozenKept = MM_Rando_Traps_TestCreationFreezes();
        if (!setRefused || frozenKept != (1 << last)) {
            printf("[TEST] FAIL: with the profile frozen the trap-type writer %s and the set read %02X, expected "
                   "%02X\n",
                   setRefused ? "refused" : "WROTE", (unsigned)frozenKept, (unsigned)(1u << last));
            ok = false;
        }
        // Reset clears the set with the options and tricks.
        Combo_MMOptionsPage_ResetAll();
        const int afterReset = MM_Rando_Traps_TestCreationFreezes();
        if (afterReset != 0) {
            printf("[TEST] FAIL: Reset MM Randomizer left the trap-type set at %02X\n", (unsigned)afterReset);
            ok = false;
        }
        clearAll();
        printf("[TEST]   %d of %d trap rows: the set creation freezes and the profile digest follow the row\n",
               followed, typeCount);
    }
    for (int t = 0; t < typeCount; t++) {
        CVarClear(MM_Rando_Traps_TestCVarName(t));
    }
    gComboCtx.mmProfileDigest = priorDigest;

    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-trap-roll-frozen\n");
    return TEST_PASS;
}
