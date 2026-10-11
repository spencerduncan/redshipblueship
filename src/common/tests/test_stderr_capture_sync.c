/**
 * Stderr-capture read-after-write lock (#901).
 *
 * The paired-first-crossing drives judge fprintf(stderr) lines through the
 * stderr tee in integration_test_hooks.cpp: fd 2 is a pipe, a detached reader
 * thread drains it and keeps the tagged lines. A drive that checks a line it
 * (or a callee) has JUST written used to read the kept lines as they were at
 * that instant, before the reader thread had caught up, so a passing creation
 * could report "creation-complete line absent" (observed once during the C5
 * fix pass, PR #896).
 *
 * This row is that race, ROM-free and many times over: it starts the tee, and
 * on every iteration writes a burst of unkept filler (so the reader has a
 * backlog to drain) followed by one kept, uniquely numbered probe line, then
 * IMMEDIATELY asks IntegrationTest_StderrCaptureLast and
 * IntegrationTest_StderrCaptureCount for that probe. Every query must see the
 * line its caller wrote before it. Without a drain in the query, the reader
 * loses most of these races and the row fails with the miss count.
 *
 * The tee's echo target (the original fd 2) is pointed at the null device for
 * the duration, so the filler never reaches the CTest log; the pipe, the reader
 * and the queries are exactly the production ones.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++); the capture API
 * is already declared via its include of integration_test_hooks.h. No display,
 * no ROM archives, no game loop.
 */

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// Enough iterations that an undrained query loses at least one race on any
// machine (on the unfixed tee it loses nearly all of them).
#define SCS_ITERATIONS 300
// Filler written before each probe: unkept (no capture tag), so it loads the
// reader without growing the kept set.
#define SCS_FILLER_LINES 8
#define SCS_FILLER_WIDTH 240

static int ScsDup(int fd) {
#ifdef _WIN32
    return _dup(fd);
#else
    return dup(fd);
#endif
}

static int ScsDup2(int from, int to) {
#ifdef _WIN32
    return _dup2(from, to);
#else
    return dup2(from, to);
#endif
}

static void ScsClose(int fd) {
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
}

TestResult Test_StderrCaptureSync(void) {
    printf("[TEST] stderr-capture-sync: a capture query sees the line its caller just wrote (#901)\n");
    fflush(stdout);

    // Point fd 2 at the null device, so the tee's echo of the filler goes nowhere.
    fflush(stderr);
    const int savedStderr = ScsDup(2);
#ifdef _WIN32
    const int nullFd = _open("NUL", _O_WRONLY | _O_BINARY);
#else
    const int nullFd = open("/dev/null", O_WRONLY);
#endif
    if (savedStderr < 0 || nullFd < 0 || ScsDup2(nullFd, 2) < 0) {
        printf("[TEST] FAIL: could not point fd 2 at the null device (saved=%d null=%d)\n", savedStderr, nullFd);
        if (nullFd >= 0) {
            ScsClose(nullFd);
        }
        if (savedStderr >= 0) {
            ScsClose(savedStderr);
        }
        return TEST_FAIL;
    }
    ScsClose(nullFd);

    const bool started = IntegrationTest_StderrCaptureStart();

    char filler[SCS_FILLER_WIDTH + 1];
    memset(filler, 'x', SCS_FILLER_WIDTH);
    filler[SCS_FILLER_WIDTH] = '\0';

    int lastMisses = 0;
    int countMisses = 0;
    int firstMiss = -1;
    char needle[96];
    char line[256];
    for (int i = 0; started && i < SCS_ITERATIONS; i++) {
        for (int f = 0; f < SCS_FILLER_LINES; f++) {
            fprintf(stderr, "[scs-load] %d.%d %s\n", i, f, filler);
        }
        // "[PFC" is a capture tag: this line is kept.
        snprintf(needle, sizeof(needle), "[PFC] capture-sync probe #%d#", i);
        fprintf(stderr, "%s written\n", needle);
        fflush(stderr);
        // The verdict, taken immediately after the write.
        const bool found = IntegrationTest_StderrCaptureLast(needle, line, sizeof(line));
        const int count = IntegrationTest_StderrCaptureCount(needle);
        if (!found) {
            lastMisses++;
        }
        if (count != 1) {
            countMisses++;
        }
        if ((!found || count != 1) && firstMiss < 0) {
            firstMiss = i;
        }
    }

    // After a full drain every probe must be there exactly once.
    IntegrationTest_StderrCaptureStop();
    const int keptAfterStop = IntegrationTest_StderrCaptureCount("[PFC] capture-sync probe #");

    fflush(stderr);
    ScsDup2(savedStderr, 2);
    ScsClose(savedStderr);

    if (!started) {
        printf("[TEST] FAIL: the stderr capture could not start\n");
        return TEST_FAIL;
    }
    printf("[TEST] stderr-capture-sync: %d probes, Last missed %d, Count missed %d (first miss at #%d), "
           "%d kept after stop\n",
           SCS_ITERATIONS, lastMisses, countMisses, firstMiss, keptAfterStop);
    if (keptAfterStop != SCS_ITERATIONS) {
        printf("[TEST] FAIL: after stop the capture kept %d probe lines, expected %d\n", keptAfterStop, SCS_ITERATIONS);
        return TEST_FAIL;
    }
    if (lastMisses != 0 || countMisses != 0) {
        printf("[TEST] FAIL: a query taken right after its caller's write missed the line (Last %d/%d, Count "
               "%d/%d): the verdict raced the capture reader thread\n",
               lastMisses, SCS_ITERATIONS, countMisses, SCS_ITERATIONS);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: every query saw the line written just before it\n");
    return TEST_PASS;
}
