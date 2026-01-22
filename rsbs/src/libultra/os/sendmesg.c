/**
 * @file sendmesg.c
 * @brief OS message queue send - unified from OoT/MM decomp
 *
 * Original sources:
 *   - games/oot/src/libultra/os/sendmesg.c
 *   - games/mm/src/libultra/os/sendmesg.c
 */

#include "ultra64.h"

s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flag) {
    register u32 saveMask = __osDisableInt();
    register s32 index;

    while (mq->validCount >= mq->msgCount) {
        if (flag == OS_MESG_BLOCK) {
            __osRunningThread->state = OS_STATE_WAITING;
            __osEnqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(saveMask);
            return -1;
        }
    }

    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;

    if (mq->mtqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->mtqueue));
    }

    __osRestoreInt(saveMask);
    return 0;
}
