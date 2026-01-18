#include "stackcheck.h"
#include "libc/stdbool.h"
#include "libc/stdint.h"

StackEntry* MM_sStackInfoListStart = NULL;
StackEntry* MM_sStackInfoListEnd = NULL;

void MM_StackCheck_Init(StackEntry* entry, void* stackBottom, void* stackTop, u32 initValue, s32 minSpace,
                     const char* name) {
    if (entry == NULL) {
        MM_sStackInfoListStart = NULL;
    } else {
        StackEntry* iter;

        entry->head = stackBottom;
        entry->tail = stackTop;
        entry->initValue = initValue;
        entry->minSpace = minSpace;
        entry->name = name;
        iter = MM_sStackInfoListStart;
        while (iter) {
            if (iter == entry) {
                return;
            }
            iter = iter->next;
        }

        entry->prev = MM_sStackInfoListEnd;
        entry->next = NULL;

        if (MM_sStackInfoListEnd) {
            MM_sStackInfoListEnd->next = entry;
        }

        MM_sStackInfoListEnd = entry;
        if (MM_sStackInfoListStart == NULL) {
            MM_sStackInfoListStart = entry;
        }

        if (entry->minSpace != -1) {
            u32* addr = entry->head;

            while (addr < (u32*)entry->tail) {
                *addr++ = entry->initValue;
            }
        }
    }
}

void MM_StackCheck_Cleanup(StackEntry* entry) {
    u32 inconsistency = false;

    if (entry->prev == NULL) {
        if (entry == MM_sStackInfoListStart) {
            MM_sStackInfoListStart = entry->next;
        } else {
            inconsistency = true;
        }
    } else {
        entry->prev->next = entry->next;
    }

    if (!entry->next) {
        if (entry == MM_sStackInfoListEnd) {
            MM_sStackInfoListEnd = entry->prev;
        } else {
            inconsistency = true;
        }
    }

    if (inconsistency) {}
}

StackStatus MM_StackCheck_GetState(StackEntry* entry) {
    u32* last;
    size_t used;
    size_t free;
    StackStatus status;

    for (last = entry->head; last < (u32*)entry->tail; last++) {
        if (entry->initValue != *last) {
            break;
        }
    }

    used = (uintptr_t)entry->tail - (uintptr_t)last;
    free = (uintptr_t)last - (uintptr_t)entry->head;

    if (free == 0) {
        status = STACK_STATUS_OVERFLOW;
    } else if ((free < (size_t)entry->minSpace) && (entry->minSpace != -1)) {
        status = STACK_STATUS_WARNING;
    } else {
        status = STACK_STATUS_OK;
    }

    return status;
}

u32 MM_StackCheck_CheckAll(void) {
    u32 ret = 0;
    StackEntry* iter = MM_sStackInfoListStart;

    while (iter != NULL) {
        StackStatus state = MM_StackCheck_GetState(iter);

        if (state != STACK_STATUS_OK) {
            ret = 1;
        }
        iter = iter->next;
    }

    return ret;
}

u32 MM_StackCheck_Check(StackEntry* entry) {
    if (entry == NULL) {
        return MM_StackCheck_CheckAll();
    } else {
        return MM_StackCheck_GetState(entry);
    }
}
