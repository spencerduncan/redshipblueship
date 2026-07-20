/**
 * Active-thread-queue contract lock (issue #385).
 *
 * WHAT BROKE
 *
 * games/oot/soh/stubs.c defined
 *
 *     OSThread* __osGetActiveQueue(void) { }
 *
 * — a non-void function with an empty body. Falling off the end is UB; in
 * practice the caller reads whatever was left in the return register. The real
 * implementation (rsbs/src/libultra/os/getactivequeue.c) could not win the
 * symbol, because it references __osActiveQueue and *nothing in the link
 * defined it*: both games' defining TUs (games/oot/src/libultra/os/createthread.c,
 * games/mm/src/libultra/os/thread.c) are excluded, and libultraship supplies
 * neither. So getactivequeue.o was unpullable and stubs.o — dragged in
 * unconditionally via osMemSize / OoT_osDestroyThread — always won.
 *
 * WHY IT MATTERS ENOUGH TO LOCK
 *
 * The consumer is the crash handler. games/oot/src/code/fault.c:537 does
 *
 *     OSThread* iter = __osGetActiveQueue();
 *     while (iter->priority != -1) { ...; iter = iter->tlnext; }
 *
 * (games/mm/src/boot/fault.c:557 is identical). So the fault handler — the one
 * piece of machinery whose entire job is to produce a usable diagnostic when
 * something else has already gone wrong — dereferenced a garbage pointer and
 * was liable to fault itself, destroying exactly the crash report this project's
 * debugging loop depends on.
 *
 * WHAT THIS TEST ASSERTS — the three properties fault.c actually needs:
 *
 *   1. NON-NULL. `iter->priority` is read before any null check, so NULL is not
 *      an acceptable answer either. This is why the fix is a tail sentinel and
 *      not `__osActiveQueue = NULL` — that would have swapped a garbage
 *      dereference for a null dereference in the same line of the same handler.
 *   2. TERMINATION. The walk reaches a node with priority == -1
 *      (OS_PRIORITY_THREADTAIL) within a bounded number of steps. Run with a
 *      hard step cap so a cyclic or garbage list fails the test instead of
 *      hanging the CTest tier.
 *   3. STABILITY. Two consecutive calls agree. A value genuinely read out of
 *      global storage is invariant; a return register is not required to be.
 *
 * Also locks the two neighbours from the same sweep whose contracts are
 * likewise consumed by fault.c and by controller setup.
 *
 * ROM-free and display-free: these are pure symbol contracts with no
 * subsystem behind them, which is the whole reason they can be locked at all.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++), like the other
 * files in this directory; the declarations below carry C linkage explicitly
 * because the definitions live in C translation units.
 */

/* Only the two headers that carry the types used below, not the libultra.h
 * umbrella: this file is #included into test_runner.cpp's C++ translation unit,
 * and gbi.h/rcp.h drag in a large macro surface that has no business there. */
#include <cstdio>
#include <libultraship/libultra/thread.h>
#include <libultraship/libultra/message.h>

extern "C" {
OSThread* __osGetActiveQueue(void);
OSThread* __osGetCurrFaultedThread(void);
s32 __osDisableInt(void);
s32 OoT_osContStartQuery(OSMesgQueue* mq);
}

/* The termination sentinel. Spelled as a bare -1 here on purpose: that is the
 * literal fault.c compares against, and restating the constant independently
 * of any header is the point of a regression lock. */
#define ACTIVEQ_THREADTAIL_PRIORITY (-1)

/* Generous relative to the real N64 thread count (single digits) and to the
 * port's (zero), but finite — the failure mode being locked out is an
 * unterminated walk, and a test that hangs is a test that reports nothing. */
#define ACTIVEQ_MAX_WALK 64

#define ACTIVEQ_CHECK(cond, msg)                \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

TestResult Test_ActiveQueue(void) {
    printf("[TEST] active-queue: __osGetActiveQueue returns a walkable list, not a return register (#385)\n");

    OSThread* head = __osGetActiveQueue();

    // (1) fault.c dereferences the result immediately, with no null check.
    ACTIVEQ_CHECK(head != NULL, "__osGetActiveQueue() returned NULL; fault.c dereferences it unconditionally");

    // (3) Stability. Checked before the walk so that a non-deterministic value
    // is reported as such rather than as a walk failure.
    OSThread* again = __osGetActiveQueue();
    ACTIVEQ_CHECK(again == head, "__osGetActiveQueue() returned two different values for two consecutive calls");

    // (2) Termination. This is fault.c's loop verbatim, plus a step cap.
    OSThread* iter = head;
    int steps = 0;
    while (iter->priority != ACTIVEQ_THREADTAIL_PRIORITY) {
        ACTIVEQ_CHECK(iter->tlnext != NULL, "active-queue walk hit a NULL tlnext before the priority == -1 tail");
        iter = iter->tlnext;
        steps++;
        ACTIVEQ_CHECK(steps < ACTIVEQ_MAX_WALK, "active-queue walk did not terminate; list is cyclic or garbage");
    }

    printf("[TEST]   walk terminated at the tail sentinel after %d step(s)\n", steps);

    // The port creates no libultra threads, so the list should be empty — the
    // head IS the sentinel. Asserted separately from termination so that a
    // future real thread list fails here loudly (a deliberate contract change)
    // rather than silently.
    ACTIVEQ_CHECK(steps == 0, "expected an empty active queue (head == tail sentinel) in the port");

    // fault.c's other input. Both handlers branch on NULL to fall back to
    // Fault_FindFaultedThread; a garbage pointer would be walked as a thread.
    ACTIVEQ_CHECK(__osGetCurrFaultedThread() == NULL, "__osGetCurrFaultedThread() must report NULL, not a register");

    // Same file, same defect class, and the value is load-bearing: the mask is
    // handed straight back to the no-op __osRestoreInt.
    ACTIVEQ_CHECK(__osDisableInt() == 0, "__osDisableInt() must return a defined interrupt mask");

    // padsetup.c aborts controller setup on a non-zero return, so garbage here
    // could fail controller init at random.
    ACTIVEQ_CHECK(OoT_osContStartQuery(NULL) == 0, "OoT_osContStartQuery() must report success (0)");

    printf("[TEST] PASS: active-queue\n");
    return TEST_PASS;
}
