/**
 * The paired-creation rows' single-bag watcher accepts the line Generate's
 * completion prints (#1135).
 *
 * IntPairedFirstCrossing and its four siblings judge the creation by the
 * stderr lines it printed, kept by the integration tee
 * (integration_test_hooks.cpp). Their watcher (OoT_Pfc_CreationBagLine,
 * games/oot/soh/GameExports_SingleExe.cpp) waited for "[OoT] creation event:
 * single bag", the line the creation event printed while it generated the
 * paired world itself. Since #1116 a creation on a world still waiting at its
 * general pass is completed by Generate's own completion
 * (OoT_PairedGen_CompleteWorld), which prints the same facts as "[OoT] paired
 * completion: single bag — ...": the tee did not keep that line and the watcher
 * did not look for it, so every paired-creation row failed with "the creation
 * never logged its single-bag line" on a correctly generated world.
 *
 * This row is that contract, ROM-free, through the production tee and the
 * production watcher:
 *   1. with neither single-bag line written, the watcher refuses with exactly
 *      the message the integration rows report;
 *   2. after the completion's line is written (as the gate logged it on
 *      2026-10-22, up to its rows-placed count), the watcher accepts it and parses 89 crossings,
 *      30 into Termina and 59 into Hyrule.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++); the capture API
 * is already declared via its include of integration_test_hooks.h. No display,
 * no ROM archives, no game loop.
 */

#include <cstdio>
#include <cstring>

extern "C" int OoT_Pfc_CreationBagLine(char* bagLine, size_t bagCap, int* total, int* intoTermina, int* intoHyrule,
                                       char* msg, size_t cap);

#define PCBL_REFUSAL "the creation never logged its single-bag line"
#define PCBL_COMPLETION_LINE                                                                                      \
    "[OoT] paired completion: single bag \xe2\x80\x94 89 crossings stored (30 OoT items in Termina, 59 MM items " \
    "in Hyrule), 265 rows placed"

TestResult Test_PfcCreationBagLine(void) {
    printf("[TEST] pfc-creation-bag-line: the paired-creation watcher accepts Generate's completion line (#1135)\n");
    fflush(stdout);

    if (!IntegrationTest_StderrCaptureStart()) {
        printf("[TEST] FAIL: the stderr capture could not start\n");
        return TEST_FAIL;
    }

    bool ok = true;
    char bagLine[512];
    char msg[640];
    int total = -1;
    int intoTermina = -1;
    int intoHyrule = -1;

    // 1. Neither single-bag line: refused, with the integration rows' message.
    memset(msg, 0, sizeof(msg));
    const int refused =
        OoT_Pfc_CreationBagLine(bagLine, sizeof(bagLine), &total, &intoTermina, &intoHyrule, msg, sizeof(msg));
    if (refused != 0 || strcmp(msg, PCBL_REFUSAL) != 0) {
        printf("[TEST] FAIL: with no single-bag line the watcher returned %d with \"%s\", expected 0 with \"%s\"\n",
               refused, msg, PCBL_REFUSAL);
        ok = false;
    }

    // 2. Generate's completion line, verbatim: accepted and parsed.
    fprintf(stderr, "%s\n", PCBL_COMPLETION_LINE);
    fflush(stderr);
    memset(msg, 0, sizeof(msg));
    total = intoTermina = intoHyrule = -1;
    const int accepted =
        OoT_Pfc_CreationBagLine(bagLine, sizeof(bagLine), &total, &intoTermina, &intoHyrule, msg, sizeof(msg));
    IntegrationTest_StderrCaptureStop();
    if (accepted == 0) {
        printf("[TEST] FAIL: the watcher refused Generate's completion line: \"%s\" (the tee kept %d such line(s))\n",
               msg, IntegrationTest_StderrCaptureCount("[OoT] paired completion: single bag"));
        ok = false;
    } else if (total != 89 || intoTermina != 30 || intoHyrule != 59 || strcmp(bagLine, PCBL_COMPLETION_LINE) != 0) {
        printf("[TEST] FAIL: the watcher accepted \"%s\" but parsed %d crossings (%d into Termina, %d into Hyrule), "
               "expected 89 (30, 59)\n",
               bagLine, total, intoTermina, intoHyrule);
        ok = false;
    }

    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: no single-bag line is refused (\"%s\"); Generate's completion line is accepted: %d crossings "
           "(%d OoT items in Termina, %d MM items in Hyrule)\n",
           PCBL_REFUSAL, total, intoTermina, intoHyrule);
    return TEST_PASS;
}
