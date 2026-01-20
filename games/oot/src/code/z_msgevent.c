#include "global.h"

void OoT_MsgEvent_SendNullTask(void) {
    s32 pad[4];
    OSScTask task;
    OSMesgQueue queue;
    OSMesg msg;
    u32 pad2[1];

    task.next = NULL;
    task.flags = OS_SC_RCP_MASK;
    task.msgQ = &queue;
    task.msg.ptr = NULL;
    task.framebuffer = NULL;
    task.list.t.type = M_NULTASK;
    osCreateMesgQueue(task.msgQ, &msg, 1);
    osSendMesgPtr(&OoT_gSchedContext.cmdQ, &task, OS_MESG_BLOCK);
    OoT_Sched_SendEntryMsg(&OoT_gSchedContext);
    osRecvMesg(&queue, NULL, OS_MESG_BLOCK);
}
