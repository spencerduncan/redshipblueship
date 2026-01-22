/**
 * @file recvmesg.c
 * @brief OS message queue receive - unified from OoT/MM decomp
 *
 * Original sources:
 *   - games/oot/src/libultra/os/recvmesg.c
 *   - games/mm/src/libultra/os/recvmesg.c
 */

#include "ultra64.h"

s32 osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flag) {
    register u32 saveMask = __osDisableInt();

    while (mq->validCount == 0) {
        if (flag == OS_MESG_NOBLOCK) {
            __osRestoreInt(saveMask);
            return -1;
        }
        __osRunningThread->state = OS_STATE_WAITING;
        __osEnqueueAndYield(&mq->mtqueue);
    }

    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    if (mq->fullqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->fullqueue));
    }

    __osRestoreInt(saveMask);
    return 0;
}
