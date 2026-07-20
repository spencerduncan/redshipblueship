/**
 * Sequence-map capacity bounds regression test (issues #371, #378).
 *
 * The rest of AudioLoad_Init needs a booted audio heap and real archives, so
 * it is unreachable from the ROM-free CTest tier. The BOUND, however, is pure
 * arithmetic, and both bugs this test locks were bound-arithmetic bugs — so
 * the capacity computation was factored into two pure functions
 * (OoT_AudioLoad_SequenceMapCapacity, MM_AudioLoad_SequenceMapCapacity) that
 * this test can call directly with no audio subsystem at all.
 *
 * What it locks:
 *
 *   1. Capacity covers the full AUTHENTIC ID RANGE, not the authentic file
 *      count. MM's sequence ids run 0x00-0x7F with 0x7A absent, so the archive
 *      yields 127 files while the highest legal id is 127 — sizing by count
 *      made `id >= gSequenceMapSize` reject the top sequence.
 *   2. Capacity leaves slack past (files + custom). Custom songs are assigned
 *      via `while (AudioCollection_HasSequenceNum(n)) n++`, which skips
 *      AudioCollection's non-sequence entries, so N custom files can consume
 *      more than N ids. #378's reported repro — 21 custom songs landing at
 *      seqNum 136 against a size of 131 — is asserted verbatim below.
 *   3. Capacity never drops below the file count, so a mod adding more
 *      sequence files than the authentic range still gets a slot per file.
 *
 * MAX_AUTHENTIC_SEQID is spelled as a literal here rather than included: the
 * two games define the same macro with different values (110 / 128), and
 * restating the contract independently is the point of a regression lock.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++), like the other
 * files in this directory; the declarations below carry C linkage explicitly
 * because both definitions live in C translation units.
 */

#include <cstddef>
#include <cstdio>

extern "C" {
size_t OoT_AudioLoad_SequenceMapCapacity(size_t authenticFileCount, size_t customCount);
size_t MM_AudioLoad_SequenceMapCapacity(size_t authenticFileCount, size_t customCount);
}

#define SEQMAP_OOT_MAX_AUTHENTIC_SEQID 110
#define SEQMAP_MM_MAX_AUTHENTIC_SEQID 128
#define SEQMAP_EXPECTED_SLACK 0xF

#define SEQMAP_CHECK(cond, msg)                        \
    do {                                               \
        if (!(cond)) {                                 \
            printf("[TEST] FAIL: %s\n", (msg));        \
            return TEST_FAIL;                          \
        }                                              \
    } while (0)

TestResult Test_SeqMapBounds(void) {
    printf("[TEST] seq-map-bounds: sequence-map capacity covers the id range + custom slack (#371, #378)\n");

    // (1) The sparse-id-space regression. MM: 127 files on disk, no custom
    // songs, highest legal id 127. Sizing by file count yields 127 slots and
    // the `id >= size` guard drops NA_BGM_END_CREDITS_SECOND_HALF.
    size_t mmSparse = MM_AudioLoad_SequenceMapCapacity(SEQMAP_MM_MAX_AUTHENTIC_SEQID - 1, 0);
    SEQMAP_CHECK(mmSparse > (size_t)(SEQMAP_MM_MAX_AUTHENTIC_SEQID - 1),
                 "MM capacity does not cover id 0x7F when the 0x7A hole shrinks the file count");
    SEQMAP_CHECK(mmSparse >= (size_t)SEQMAP_MM_MAX_AUTHENTIC_SEQID,
                 "MM capacity is below MAX_AUTHENTIC_SEQID");

    size_t ootSparse = OoT_AudioLoad_SequenceMapCapacity(0, 0);
    SEQMAP_CHECK(ootSparse >= (size_t)SEQMAP_OOT_MAX_AUTHENTIC_SEQID,
                 "OoT capacity is below MAX_AUTHENTIC_SEQID on an empty archive");

    // (2) #378's reported repro, verbatim: OoT with 110 authentic files and 21
    // custom songs. The HasSequenceNum skip walks past AudioCollection's
    // instrument entries at 130-135, so the 21st song is assigned seqNum 136
    // while the old size was 110 + 21 == 131. 136 must be in bounds.
    size_t ootRepro = OoT_AudioLoad_SequenceMapCapacity(SEQMAP_OOT_MAX_AUTHENTIC_SEQID, 21);
    SEQMAP_CHECK(ootRepro > 136, "OoT capacity rejects seqNum 136 (the 21-custom-song repro from #378)");
    SEQMAP_CHECK(ootRepro >= (size_t)(SEQMAP_OOT_MAX_AUTHENTIC_SEQID + 21 + SEQMAP_EXPECTED_SLACK),
                 "OoT capacity does not carry the non-consecutive-id slack");

    // The same shape must hold on the MM side, which previously had no slack
    // at all — there the overflow was an out-of-bounds heap write.
    size_t mmRepro = MM_AudioLoad_SequenceMapCapacity(SEQMAP_MM_MAX_AUTHENTIC_SEQID - 1, 21);
    SEQMAP_CHECK(mmRepro >= (size_t)(SEQMAP_MM_MAX_AUTHENTIC_SEQID + 21 + SEQMAP_EXPECTED_SLACK),
                 "MM capacity does not carry the non-consecutive-id slack");

    // (3) Every custom file gets a slot: capacity grows one-for-one with the
    // custom count, so the map cannot be starved by installing more songs.
    for (size_t custom = 0; custom < 64; custom++) {
        size_t oot = OoT_AudioLoad_SequenceMapCapacity(SEQMAP_OOT_MAX_AUTHENTIC_SEQID, custom);
        size_t mm = MM_AudioLoad_SequenceMapCapacity(SEQMAP_MM_MAX_AUTHENTIC_SEQID, custom);

        SEQMAP_CHECK(oot >= (size_t)SEQMAP_OOT_MAX_AUTHENTIC_SEQID + custom,
                     "OoT capacity does not grow with the custom-song count");
        SEQMAP_CHECK(mm >= (size_t)SEQMAP_MM_MAX_AUTHENTIC_SEQID + custom,
                     "MM capacity does not grow with the custom-song count");
    }

    // (4) A mod shipping more sequence files than the authentic range must
    // still get a slot per file — clamping to MAX_AUTHENTIC_SEQID would
    // reintroduce the overflow from the other side.
    size_t ootModded = OoT_AudioLoad_SequenceMapCapacity(4096, 8);
    SEQMAP_CHECK(ootModded >= 4096 + 8, "OoT capacity clamps below an oversized authentic file count");

    size_t mmModded = MM_AudioLoad_SequenceMapCapacity(4096, 8);
    SEQMAP_CHECK(mmModded >= 4096 + 8, "MM capacity clamps below an oversized authentic file count");

    printf("[TEST] PASS: sequence-map capacity bounds hold for both games\n");
    return TEST_PASS;
}

#undef SEQMAP_CHECK
