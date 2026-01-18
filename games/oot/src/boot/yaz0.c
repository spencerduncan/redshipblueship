#include "global.h"
#include <string.h>

u8 OoT_sYaz0DataBuffer[0x400];
uintptr_t OoT_sYaz0CurDataEnd;
uintptr_t OoT_sYaz0CurRomStart;
u32 OoT_sYaz0CurSize;
uintptr_t OoT_sYaz0MaxPtr;

void* OoT_Yaz0_FirstDMA(void) {
    u32 pad0;
    u32 pad1;
    u32 dmaSize;
    u32 curSize;

    OoT_sYaz0MaxPtr = OoT_sYaz0CurDataEnd - 0x19;

    curSize = OoT_sYaz0CurDataEnd - (uintptr_t)OoT_sYaz0DataBuffer;
    dmaSize = (curSize > OoT_sYaz0CurSize) ? OoT_sYaz0CurSize : curSize;

    OoT_DmaMgr_DmaRomToRam(OoT_sYaz0CurRomStart, OoT_sYaz0DataBuffer, dmaSize);
    OoT_sYaz0CurRomStart += dmaSize;
    OoT_sYaz0CurSize -= dmaSize;
    return OoT_sYaz0DataBuffer;
}

void* OoT_Yaz0_NextDMA(void* curSrcPos) {
    u8* dst;
    u32 restSize;
    u32 dmaSize;

    restSize = OoT_sYaz0CurDataEnd - (uintptr_t)curSrcPos;
    dst = (restSize & 7) ? (OoT_sYaz0DataBuffer - (restSize & 7)) + 8 : OoT_sYaz0DataBuffer;

    memcpy(dst, curSrcPos, restSize);
    dmaSize = (OoT_sYaz0CurDataEnd - (uintptr_t)dst) - restSize;
    if (OoT_sYaz0CurSize < dmaSize) {
        dmaSize = OoT_sYaz0CurSize;
    }

    if (dmaSize != 0) {
        OoT_DmaMgr_DmaRomToRam(OoT_sYaz0CurRomStart, (uintptr_t)dst + restSize, dmaSize);
        OoT_sYaz0CurRomStart += dmaSize;
        OoT_sYaz0CurSize -= dmaSize;
        if (!OoT_sYaz0CurSize) {
            OoT_sYaz0MaxPtr = (uintptr_t)dst + restSize + dmaSize;
        }
    }

    return dst;
}

void OoT_Yaz0_DecompressImpl(Yaz0Header* hdr, u8* dst) {
    u32 bitIdx = 0;
    u8* src = (u8*)hdr->data;
    u8* dstEnd = dst + hdr->decSize;
    u32 chunkHeader;
    u32 nibble;
    u8* backPtr;
    u32 chunkSize;
    u32 off;

    do {
        if (bitIdx == 0) {
            if ((OoT_sYaz0MaxPtr < (uintptr_t)src) && (OoT_sYaz0CurSize != 0)) {
                src = OoT_Yaz0_NextDMA(src);
            }

            chunkHeader = *src++;
            bitIdx = 8;
        }

        if (chunkHeader & (1 << 7)) { // uncompressed
            *dst = *src;
            dst++;
            src++;
        } else { // compressed
            off = ((*src & 0xF) << 8 | *(src + 1));
            nibble = *src >> 4;
            backPtr = dst - off;
            src += 2;

            chunkSize = (nibble == 0)              // N = chunkSize; B = back offset
                            ? (u32)(*src++ + 0x12) // 3 bytes 0B BB NN
                            : nibble + 2;          // 2 bytes NB BB

            do {
                *dst++ = *(backPtr++ - 1);
                chunkSize--;
            } while (chunkSize != 0);
        }
        chunkHeader <<= 1;
        bitIdx--;
    } while (dst != dstEnd);
}

void OoT_Yaz0_Decompress(uintptr_t romStart, void* dst, size_t size) {
    OoT_sYaz0CurRomStart = romStart;
    OoT_sYaz0CurSize = size;
    OoT_sYaz0CurDataEnd = OoT_sYaz0DataBuffer + sizeof(OoT_sYaz0DataBuffer);
    OoT_Yaz0_DecompressImpl(OoT_Yaz0_FirstDMA(), dst);
}
