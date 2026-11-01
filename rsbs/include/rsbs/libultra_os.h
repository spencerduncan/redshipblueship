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

#endif /* RSBS_LIBULTRA_OS_H */
