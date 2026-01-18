#include "gamealloc.h"

#include "system_malloc.h"

void MM_GameAlloc_Log(GameAlloc* this) {
    GameAllocEntry* iter = this->base.next;

    while (iter != &this->base) {
        iter = iter->next;
    }
}

void* MM_GameAlloc_Malloc(GameAlloc* this, size_t size) {
    GameAllocEntry* ptr = MM_SystemArena_Malloc(size + sizeof(GameAllocEntry));

    if (ptr != NULL) {
        ptr->size = size;
        ptr->prev = this->head;
        this->head->next = ptr;
        this->head = ptr;
        ptr->next = &this->base;
        this->base.prev = this->head;
        return ptr + 1;
    } else {
        return NULL;
    }
}

void MM_GameAlloc_Free(GameAlloc* this, void* data) {
    GameAllocEntry* ptr;

    if (data != NULL) {
        ptr = &((GameAllocEntry*)data)[-1];
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
        this->head = this->base.prev;
        MM_SystemArena_Free(ptr);
    }
}

void MM_GameAlloc_Cleanup(GameAlloc* this) {
    GameAllocEntry* next = this->base.next;
    GameAllocEntry* cur;

    while (&this->base != next) {
        cur = next;
        next = next->next;
        MM_SystemArena_Free(cur);
    }

    this->head = &this->base;
    this->base.next = &this->base;
    this->base.prev = &this->base;
}

void MM_GameAlloc_Init(GameAlloc* this) {
    this->head = &this->base;
    this->base.next = &this->base;
    this->base.prev = &this->base;
}
