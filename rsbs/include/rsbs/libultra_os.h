/**
 * @file libultra_os.h
 * @brief libultra OS types and declarations for rsbs
 *
 * This header provides the necessary type declarations and externs
 * for the unified libultra/os functions in rsbs.
 */

#ifndef RSBS_LIBULTRA_OS_H
#define RSBS_LIBULTRA_OS_H

#include <libultraship/libultra/thread.h>
#include <libultraship/libultra/interrupt.h>

/* Thread-related extern declarations */
extern OSThread* __osActiveQueue;
extern OSThread* __osRunningThread;

/* Internal functions */
void __osDequeueThread(OSThread** queue, OSThread* thread);
void __osDispatchThread(void);
/* Defined in games/oot/soh/stubs.c (signatures match games/oot/include/functions.h).
 * destroythread, jammesg, recvmesg, sendmesg and seteventmesg call them; without
 * these prototypes the calls are implicit declarations, which C99 and later
 * forbid and AppleClang rejects as an error. */
s32 __osDisableInt(void);
void __osRestoreInt(s32);

#endif /* RSBS_LIBULTRA_OS_H */
