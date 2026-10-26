/**
 * @file test_skeleton_patcher_layout.c
 * @brief Lock for OoT's skeleton patcher writing past Majora's Mask SkelAnimes (#1176).
 *
 * In the single exe both games call Ship of Harkinian's
 * ResourceMgr_LoadSkeletonByName (2Ship's copy in BenPort.cpp is excluded), so
 * every Majora's Mask SkelAnime_Init registers its SkelAnime with
 * SOH::SkeletonPatcher. An Alt Assets change runs UpdateSkeletons, which writes
 * `skeleton` (same offset in both games) and then `skeletonHeader`, the
 * trailing pointer SoH appended to OoT's SkelAnime. Majora's Mask's SkelAnime
 * ends before that field, so the second write lands in whatever the owning
 * actor declares next.
 *
 * SkeletonPatcherLayout_RunHeadless is the ROM-free `skeleton-patcher-mm-layout`
 * row, included at FILE SCOPE by test_runner.cpp (compiled as C++). It asserts:
 *   1. OoT's SkelAnime is MM's plus exactly one trailing pointer (both sizes are
 *      exported by the games' GameExports TUs, which static_assert the field
 *      offsets the patcher relies on);
 *   2. applying an MM-tagged entry to an MM-sized SkelAnime followed by a guard
 *      word writes `skeleton` and leaves the guard untouched;
 *   3. applying an OoT-tagged entry writes both `skeleton` and `skeletonHeader`;
 *   4. ResourceMgr_LoadSkeletonByName tags a registration with the game that is
 *      running: MM under MM, OoT under OoT, and OoT when no game runs yet.
 * Leg 4 needs the shared Ship::Context (the wrapper brings it up); the path it
 * loads does not exist, which still registers the SkelAnime, as a real
 * SkelAnime_Init does whether or not the skeleton resolved.
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../context.h"
#include "../game.h"

extern "C" size_t MM_SkelAnime_SizeOf(void);
extern "C" size_t OoT_SkelAnime_SizeOf(void);
extern "C" void SkeletonPatcher_ApplyForTest(void* skelAnime, int game, void** skeletonSegment,
                                             uintptr_t resourcePointer);
extern "C" int SkeletonPatcher_RegisteredGameForTest(const void* skelAnime);
// C linkage, so the opaque pointer types here link against OoT's SkelAnime* definitions.
extern "C" void* ResourceMgr_LoadSkeletonByName(const char* path, void* skelAnime);
extern "C" void ResourceMgr_UnregisterSkeleton(void* skelAnime);

namespace {

constexpr unsigned char kSplPattern = 0xA5;
constexpr uint64_t kSplGuard = 0x5A5A5A5A5A5A5A5AULL;
// Both GameExports TUs static_assert `skeleton` sits right after the four u8 fields.
constexpr size_t kSplSkeletonOffset = sizeof(void*);

struct SplBuffer {
    alignas(16) unsigned char bytes[512];
};

uintptr_t SplReadPointerAt(const unsigned char* base, size_t offset) {
    uintptr_t value = 0;
    memcpy(&value, base + offset, sizeof(value));
    return value;
}

} // namespace

int SkeletonPatcherLayout_RunHeadless(void) {
    int failures = 0;
    const size_t mmSize = MM_SkelAnime_SizeOf();
    const size_t ootSize = OoT_SkelAnime_SizeOf();
    printf("[TEST] SkelAnime sizes: OoT %zu, MM %zu\n", ootSize, mmSize);

    // 1. The layouts differ by exactly the trailing skeletonHeader pointer.
    if (ootSize != mmSize + sizeof(void*)) {
        printf("[TEST] FAIL: OoT SkelAnime (%zu) is not MM's (%zu) plus one pointer\n", ootSize, mmSize);
        failures++;
    }
    if (mmSize + sizeof(uint64_t) > sizeof(SplBuffer::bytes) || ootSize > sizeof(SplBuffer::bytes)) {
        printf("[TEST] FAIL: SkelAnime sizes exceed the row's buffer\n");
        return failures + 1;
    }

    static void* sSegment[4];
    void** const segment = sSegment;
    const uintptr_t resource = (uintptr_t)0x1234567890ABCDEFULL;

    // 2. MM-tagged entry on an MM-sized SkelAnime followed by a guard word.
    {
        SplBuffer buf;
        memset(buf.bytes, kSplPattern, sizeof(buf.bytes));
        memcpy(buf.bytes + mmSize, &kSplGuard, sizeof(kSplGuard));
        SkeletonPatcher_ApplyForTest(buf.bytes, GAME_MM, segment, resource);

        const uintptr_t skeleton = SplReadPointerAt(buf.bytes, kSplSkeletonOffset);
        if (skeleton != (uintptr_t)segment) {
            printf("[TEST] FAIL: MM entry: `skeleton` not written (0x%llx)\n", (unsigned long long)skeleton);
            failures++;
        }
        uint64_t guard = 0;
        memcpy(&guard, buf.bytes + mmSize, sizeof(guard));
        if (guard != kSplGuard) {
            printf("[TEST] FAIL: MM entry: the word after MM's SkelAnime was overwritten (0x%llx, resource 0x%llx)\n",
                   (unsigned long long)guard, (unsigned long long)resource);
            failures++;
        }
    }

    // 3. OoT-tagged entry writes both fields.
    {
        SplBuffer buf;
        memset(buf.bytes, kSplPattern, sizeof(buf.bytes));
        SkeletonPatcher_ApplyForTest(buf.bytes, GAME_OOT, segment, resource);

        const uintptr_t skeleton = SplReadPointerAt(buf.bytes, kSplSkeletonOffset);
        const uintptr_t header = SplReadPointerAt(buf.bytes, ootSize - sizeof(void*));
        if (skeleton != (uintptr_t)segment) {
            printf("[TEST] FAIL: OoT entry: `skeleton` not written (0x%llx)\n", (unsigned long long)skeleton);
            failures++;
        }
        if (header != resource) {
            printf("[TEST] FAIL: OoT entry: `skeletonHeader` not written (0x%llx)\n", (unsigned long long)header);
            failures++;
        }
    }

    // 4. Registrations carry the running game.
    {
        struct Leg {
            GameId running;
            int expected;
            const char* name;
        };
        const Leg legs[] = {
            { GAME_MM, GAME_MM, "under MM" },
            { GAME_OOT, GAME_OOT, "under OoT" },
            { GAME_NONE, GAME_OOT, "before any game runs" },
        };
        const GameId savedGame = Context_GetCurrentGame();
        for (const Leg& leg : legs) {
            SplBuffer buf;
            memset(buf.bytes, 0, sizeof(buf.bytes));
            Context_SetCurrentGame(leg.running);
            ResourceMgr_LoadSkeletonByName("rsbs_test/skeleton_patcher_layout/gMissingSkel", buf.bytes);
            const int got = SkeletonPatcher_RegisteredGameForTest(buf.bytes);
            ResourceMgr_UnregisterSkeleton(buf.bytes);
            if (got != leg.expected) {
                printf("[TEST] FAIL: a registration %s was tagged %d, expected %d\n", leg.name, got, leg.expected);
                failures++;
            }
            if (SkeletonPatcher_RegisteredGameForTest(buf.bytes) != -1) {
                printf("[TEST] FAIL: the registration %s outlived its unregister\n", leg.name);
                failures++;
            }
        }
        Context_SetCurrentGame(savedGame);
    }

    return failures;
}
