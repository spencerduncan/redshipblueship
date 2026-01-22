/**
 * @file createmesgqueue.c
 * @brief OS message queue creation - unified from OoT/MM decomp
 *
 * Original sources:
 *   - games/oot/src/libultra/os/createmesgqueue.c
 *   - games/mm/src/libultra/os/createmesgqueue.c
 */

#include "ultra64.h"

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msg, s32 count) {
    mq->mtqueue = (OSThread*)__osThreadTail;
    mq->fullqueue = (OSThread*)__osThreadTail;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msg;
}
