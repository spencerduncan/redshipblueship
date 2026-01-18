#include "buffers.h"
#include "z64.h"
#include <assert.h>
#include <stdlib.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

u8* MM_gAudioHeap;
u8* MM_gSystemHeap;

void MM_Heaps_Alloc(void) {
#ifdef _MSC_VER
    MM_gAudioHeap = (u8*)_aligned_malloc(AUDIO_HEAP_SIZE, 0x10);
    MM_gSystemHeap = (u8*)_aligned_malloc(SYSTEM_HEAP_SIZE, 0x10);
#elif defined(__unix__) || defined(__APPLE__)
    MM_gAudioHeap = mmap(NULL, AUDIO_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    MM_gSystemHeap = mmap(NULL, SYSTEM_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(MM_gAudioHeap != MAP_FAILED);
    assert(MM_gSystemHeap != MAP_FAILED);
#else
    MM_gAudioHeap = (u8*)memalign(0x10, AUDIO_HEAP_SIZE);
    MM_gSystemHeap = (u8*)memalign(0x10, SYSTEM_HEAP_SIZE);
#endif

    assert(MM_gAudioHeap != NULL);
    assert(MM_gSystemHeap != NULL);
}

void MM_Heaps_Free(void) {
#ifdef _MSC_VER
    _aligned_free(MM_gAudioHeap);
    _aligned_free(MM_gSystemHeap);
#elif defined(__unix__) || defined(__APPLE__)
    munmap(MM_gAudioHeap, AUDIO_HEAP_SIZE);
    munmap(MM_gSystemHeap, SYSTEM_HEAP_SIZE);
#else
    free(MM_gAudioHeap);
    free(MM_gSystemHeap);
#endif
}
