#include "z64.h"
#include <assert.h>
#ifndef __APPLE__
#include <malloc.h>
#endif
#include <stdlib.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

u8* OoT_gAudioHeap;
u8* OoT_gSystemHeap;

void OoT_Heaps_Alloc(void) {
#ifdef _MSC_VER
    OoT_gAudioHeap = (u8*)_aligned_malloc(AUDIO_HEAP_SIZE, 0x10);
    OoT_gSystemHeap = (u8*)_aligned_malloc(SYSTEM_HEAP_SIZE, 0x10);
#elif defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L)
    if (posix_memalign((void**)&OoT_gAudioHeap, 0x10, AUDIO_HEAP_SIZE) != 0)
        OoT_gAudioHeap = NULL;
    if (posix_memalign((void**)&OoT_gSystemHeap, 0x10, SYSTEM_HEAP_SIZE) != 0)
        OoT_gSystemHeap = NULL;
#else
    OoT_gAudioHeap = (u8*)memalign(0x10, AUDIO_HEAP_SIZE);
    OoT_gSystemHeap = (u8*)memalign(0x10, SYSTEM_HEAP_SIZE);
#endif

    assert(OoT_gAudioHeap != NULL);
    assert(OoT_gSystemHeap != NULL);
}

void OoT_Heaps_Free(void) {
#ifdef _MSC_VER
    _aligned_free(OoT_gAudioHeap);
    _aligned_free(OoT_gSystemHeap);
#else
    free(OoT_gAudioHeap);
    free(OoT_gSystemHeap);
#endif
}
