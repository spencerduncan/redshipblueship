/**
 * @file dequeuethread.c
 * @brief Remove a thread from a queue
 *
 * Implementation from OoT (MM does not have this file separately).
 */

#include "rsbs/libultra_os.h"

void __osDequeueThread(OSThread** queue, OSThread* thread) {
    register OSThread** ptr = queue;
    register OSThread* current = *ptr;

    while (current != NULL) {
        if (current == thread) {
            *ptr = thread->next;
            return;
        }
        ptr = &current->next;
        current = *ptr;
    }
}
