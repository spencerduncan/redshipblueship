/**
 * @file threadqueue.c
 * @brief Storage for the libultra active-thread list (issue #385)
 *
 * WHY THIS FILE EXISTS
 *
 * `__osActiveQueue` and `__osRunningThread` are defined upstream in
 * games/oot/src/libultra/os/createthread.c and games/mm/src/libultra/os/thread.c.
 * Both translation units are excluded from the single-executable build
 * unconditionally (games/oot/CMakeLists.txt filters src/libultra/os/,
 * games/mm/CMakeLists.txt likewise), and libultraship supplies neither symbol.
 *
 * That left the whole list *undefined anywhere in the link*, which in turn made
 * rsbs's getactivequeue.o unpullable: it defines only `__osGetActiveQueue` and
 * references `__osActiveQueue`, so any link that reached for it would have
 * failed with `undefined reference to __osActiveQueue`. In practice the linker
 * never got there — soh_port's stubs.o is dragged in unconditionally (osMemSize,
 * OoT_osDestroyThread) and its empty `OSThread* __osGetActiveQueue(void) { }`
 * body won the symbol first. The fault handler then read a *return register*
 * as a thread pointer and walked it. See issue #385.
 *
 * Defining the storage here makes getactivequeue.o (and destroythread.o,
 * getthreadid.o, getthreadpri.o) linkable, which is the precondition for
 * deleting the stub.
 *
 * WHY A SENTINEL AND NOT NULL
 *
 * The consumers do not null-check. games/oot/src/code/fault.c walks
 *
 *     OSThread* iter = __osGetActiveQueue();
 *     while (iter->priority != -1) { ...; iter = iter->tlnext; }
 *
 * and rsbs/src/libultra/os/destroythread.c has the same `priority != -1` loop.
 * The N64 termination condition is a tail node with priority
 * OS_PRIORITY_THREADTAIL (-1), not a NULL pointer — so `__osActiveQueue = NULL`
 * would trade a garbage-pointer dereference for a null dereference in exactly
 * the same crash handler. The empty list is therefore the sentinel itself:
 * every walk terminates on the first iteration and yields "no faulted thread",
 * which is the truthful answer for a port that runs no libultra threads.
 */

#include "rsbs/libultra_os.h"
#include <stddef.h>

/* libultraship's thread.h stops at OS_PRIORITY_IDLE; the tail priority is part
 * of the internal (osint.h) contract that both games' fault handlers encode as
 * a bare -1. Spelled once here so the sentinel and the walk agree by name. */
#ifndef OS_PRIORITY_THREADTAIL
#define OS_PRIORITY_THREADTAIL (-1)
#endif

/* Positional partial initialisation (next, priority); every remaining member is
 * zero-initialised by C's aggregate rules. Kept positional rather than
 * designated so the C11 requirement stays as low as possible across MSVC/GCC. */
static OSThread sThreadTail = { NULL, OS_PRIORITY_THREADTAIL };

/* An empty active list: the head *is* the tail sentinel. */
OSThread* __osActiveQueue = &sThreadTail;

/* No libultra thread is ever dispatched in the port. NULL is the correct value
 * and is what osGetThreadId/osGetThreadPri already special-case ("current
 * thread"); it exists so those objects and destroythread.o are linkable rather
 * than dead weight that would break the link the moment anything reached them. */
OSThread* __osRunningThread = NULL;
