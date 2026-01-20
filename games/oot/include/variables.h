#ifndef VARIABLES_H
#define VARIABLES_H

#include "z64.h"
#include "segment_symbols.h"

#ifdef __cplusplus
extern "C"
{
#endif

	extern u32 osTvType;
	extern u32 osRomBase;
	extern u32 osResetType;
	extern u32 osMemSize;
	extern u8 osAppNmiBuffer[0x40];

	extern u8 D_80009320[];
	extern u8 D_800093F0[];
	extern s8 D_80009430;
	extern u32 D_80009460;
	extern u32 OoT_gDmaMgrDmaBuffSize;
	extern vu8 gViConfigUseDefault;
	extern u8 OoT_gViConfigAdditionalScanLines;
	extern u32 OoT_gViConfigFeatures;
	extern f32 OoT_gViConfigXScale;
	extern f32 OoT_gViConfigYScale;
	extern OSPiHandle* OoT_gCartHandle;
	extern u32 __osPiAccessQueueEnabled;
	extern OSViMode OoT_osViModePalLan1;
	extern s32 OoT_osViClock;
	extern u32 __osShutdown;
	extern OSHWIntr __OSGlobalIntMask;
	extern OSThread* __osThreadTail[];
	extern OSThread* __osRunQueue;
	extern OSThread* __osActiveQueue;
	extern OSThread* __osRunningThread;
	extern OSThread* __osFaultedThread;
	extern OSPiHandle* __osPiTable;
	extern OSPiHandle* __osCurrentHandle[];
	extern OSTimer* __osTimerList;
	extern OSViMode OoT_osViModeNtscLan1;
	extern OSViMode OoT_osViModeMpalLan1;
	extern OSViContext* __osViCurr;
	extern OSViContext* __osViNext;
	extern OSViMode OoT_osViModeFpalLan1;
	extern u32 __additional_scanline;
	extern const char OoT_gBuildVersion[];
	extern u16 OoT_gBuildVersionMajor;
	extern u16 OoT_gBuildVersionMinor;
	extern u16 OoT_gBuildVersionPatch;
	extern const char OoT_gGitBranch[];
    extern const char OoT_gGitCommitHash[];
	extern u8 OoT_gGitCommitTag[];
	extern u8 OoT_gBuildTeam[];
	extern u8 OoT_gBuildDate[];
	extern u8 OoT_gBuildMakeOption[];
	extern OSMesgQueue gPiMgrCmdQ;
	extern OSViMode OoT_gViConfigMode;
	extern u8 D_80013960;
	extern OSMesgQueue __osPiAccessQueue;
	extern OSPiHandle __Dom1SpeedParam;
	extern OSPiHandle __Dom2SpeedParam;
	extern OSTime __osCurrentTime;
	extern u32 __osBaseCounter;
	extern u32 __osViIntrCount;
	extern u32 __osTimerCounter;
	extern DmaEntry gDmaDataTable[0x60C];
	extern u64 D_801120C0[];
	extern u8 D_80113070[];
	extern u64 gJpegUCode[];
	extern EffectSsOverlay OoT_gEffectSsOverlayTable[EFFECT_SS_TYPE_MAX];
	extern Gfx D_80116280[];
	extern s32 gDbgCamEnabled;
	extern GameStateOverlay OoT_gGameStateOverlayTable[6];
	extern u8 OoT_gWeatherMode;
	extern u8 D_8011FB34;
	extern u8 D_8011FB38;
	extern u8 gSkyboxBlendingEnabled;
	extern u16 gTimeIncrement;
	extern struct_8011FC1C D_8011FC1C[][9];
	extern SkyboxFile gSkyboxFiles[];
	extern s32 gZeldaArenaLogSeverity;
	extern MapData gMapDataTable;
	extern s16 gSpoilingItems[3];
	extern s16 gSpoilingItemReverts[3];
	extern FlexSkeletonHeader* gPlayerSkelHeaders[2];
	extern u8 OoT_gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX];
	extern Gfx* gPlayerLeftHandBgsDLs[];
	extern Gfx* OoT_gPlayerLeftHandOpenDLs[];
	extern Gfx* OoT_gPlayerLeftHandClosedDLs[];
	extern Gfx* gPlayerLeftHandBoomerangDLs[];
	extern Gfx OoT_gCullBackDList[];
	extern Gfx OoT_gCullFrontDList[];
	extern Gfx OoT_gEmptyDL[];
	extern u32 OoT_gBitFlags[32];
	extern u16 OoT_gEquipMasks[4];
	extern u16 OoT_gEquipNegMasks[4];
	extern u32 OoT_gUpgradeMasks[8];
	extern u32 OoT_gUpgradeNegMasks[8];
	extern u8 OoT_gEquipShifts[4];
	extern u8 OoT_gUpgradeShifts[8];
	extern u16 OoT_gUpgradeCapacities[8][4];
	extern u32 gGsFlagsMasks[4];
	extern u32 gGsFlagsShifts[4];
	extern void* OoT_gItemIcons[157];
	extern u8 gItemAgeReqs[];
	extern u8 gSlotAgeReqs[];
	extern u8 OoT_gItemSlots[56];
	extern void (*gSceneCmdHandlers[SCENE_CMD_ID_MAX])(PlayState*, SceneCmd*);
	extern s16 gLinkObjectIds[2];
	extern u32 OoT_gObjectTableSize;
	extern RomFile OoT_gObjectTable[OBJECT_ID_MAX];
	extern EntranceInfo gEntranceTable[ENTR_MAX];
	extern SceneTableEntry OoT_gSceneTable[SCENE_ID_MAX];
	extern u16 gSramSlotOffsets[];
	// 4 16-colors palettes
	extern u64 gMojiFontTLUTs[4][4]; // original name: "moji_tlut"
	extern u64 gMojiFontTex[]; // original name: "font_ff"
	extern KaleidoMgrOverlay OoT_gKaleidoMgrOverlayTable[KALEIDO_OVL_MAX];
	extern KaleidoMgrOverlay* OoT_gKaleidoMgrCurOvl;
	extern u8 gBossMarkState;
	extern void* gDebugCutsceneScript;
	extern s32 OoT_gScreenWidth;
	extern s32 OoT_gScreenHeight;
	extern Mtx gMtxClear;
	extern MtxF gMtxFClear;
	extern u32 gIsCtrlr2Valid;
	extern vu32 OoT_gIrqMgrResetStatus;
	extern volatile OSTime OoT_gIrqMgrRetraceTime;
	extern s16* OoT_gWaveSamples[9];
	extern f32 OoT_gBendPitchOneOctaveFrequencies[256];
	extern f32 OoT_gBendPitchTwoSemitonesFrequencies[256];
	extern f32 gNoteFrequencies[];
	extern u8 OoT_gDefaultShortNoteVelocityTable[16];
	extern u8 OoT_gDefaultShortNoteGateTimeTable[16];
	extern AdsrEnvelope OoT_gDefaultEnvelope[4];
	extern NoteSubEu gZeroNoteSub;
	extern NoteSubEu gDefaultNoteSub;
	extern u16 gHeadsetPanQuantization[64];
	extern s16 D_8012FBA8[];
	extern f32 OoT_gHeadsetPanVolume[128];
	extern f32 OoT_gStereoPanVolume[128];
	extern f32 OoT_gDefaultPanVolume[128];
	extern s16 sLowPassFilterData[16 * 8];
	extern s16 sHighPassFilterData[15 * 8];
	extern s32 gAudioContextInitalized;
	extern u8 gIsLargeSoundBank[7];
	extern u8 OoT_gChannelsPerBank[4][7];
	extern u8 OoT_gUsedChannelsPerBank[4][7];
	extern u8 OoT_gMorphaTransposeTable[16];
	extern u8* OoT_gFrogsSongPtr;
	extern OcarinaNote* gScarecrowCustomSongPtr;
	extern u8* OoT_gScarecrowSpawnSongPtr;
	extern OcarinaSongInfo gOcarinaSongNotes[];
	extern SoundParams* gSoundParams[7];
	extern char D_80133390[];
	extern char D_80133398[];
	extern SoundBankEntry* gSoundBanks[7];
	extern u8 OoT_gSfxChannelLayout;
	extern Vec3f OoT_gSfxDefaultPos;
	extern f32 OoT_gSfxDefaultFreqAndVolScale;
	extern s8 OoT_gSfxDefaultReverb;
	extern u8 D_801333F0;
	extern u8 gAudioSfxSwapOff;
	extern u8 D_80133408;
	extern u8 D_8013340C;
	extern u8 OoT_gAudioSpecId;
	extern u8 D_80133418;
	extern AudioSpec OoT_gAudioSpecs[18];
	extern s32 OoT_gOverlayLogSeverity;
	extern s32 gSystemArenaLogSeverity;
	extern u8 __osPfsInodeCacheBank;
	extern s32 __osPfsLastChannel;
	extern u8 gWalkSpeedToggle1;
	extern u8 gWalkSpeedToggle2;
	extern f32 iceTrapScale;
	extern f32 triforcePieceScale;
	extern f32 mysteryItemScale;
	
	extern const s16 D_8014A6C0[];
#define gTatumsPerBeat (D_8014A6C0[1])
	extern const AudioContextInitSizes D_8014A6C4;
	extern s16 OoT_gOcarinaSongItemMap[];
	extern u8 D_80155F50[];
	extern u8 D_80157580[];
	extern u8 D_801579A0[];
	extern u64 gJpegUCodeData[];

	extern SaveContext gSaveContext;
	extern GameInfo* gGameInfo;
	extern u16 D_8015FCC0;
	extern u16 D_8015FCC2;
	extern u16 D_8015FCC4;
	extern u8 D_8015FCC8;
	extern u8 gCustomLensFlareOn;
	extern Vec3f gCustomLensFlarePos;
	extern s16 gLensFlareScale;
	extern f32 gLensFlareColorIntensity;
	extern s16 gLensFlareScreenFillAlpha;
	extern LightningStrike gLightningStrike;
	extern MapData* gMapData;
	extern f32 gBossMarkScale;
	extern PauseMapMarksData* gLoadedPauseMarkDataTable;
	extern s32 gTrnsnUnkState;
	extern Color_RGBA8_u32 OoT_gVisMonoColor;
	extern PreNmiBuff* gAppNmiBufferPtr;
	extern SchedContext OoT_gSchedContext;
	extern PadMgr OoT_gPadMgr;
	extern uintptr_t OoT_gSegments[NUM_SEGMENTS];
	extern volatile OSTime D_8016A520;
	extern volatile OSTime D_8016A528;
	extern volatile OSTime D_8016A530;
	extern volatile OSTime D_8016A538;
	extern volatile OSTime D_8016A540;
	extern volatile OSTime D_8016A548;
	extern volatile OSTime D_8016A550;
	extern volatile OSTime D_8016A558;
	extern volatile OSTime gRSPAudioTotalTime;
	extern volatile OSTime gRSPGFXTotalTime;
	extern volatile OSTime gRSPOtherTotalTime;
	extern volatile OSTime gRDPTotalTime;
	extern FaultThreadStruct gFaultStruct;

	extern ActiveSound gActiveSounds[7][MAX_CHANNELS_PER_BANK]; // total size = 0xA8
	extern u8 gSoundBankMuted[];
	extern u8 D_801333F0;
	extern u8 gAudioSfxSwapOff;
	extern u16 gAudioSfxSwapSource[10];
	extern u16 gAudioSfxSwapTarget[10];
	extern u8 gAudioSfxSwapMode[10];
	extern ActiveSequence OoT_gActiveSeqs[4];
	extern AudioContext gAudioContext;
	extern void(*D_801755D0)(void);

	extern u32 __osMalloc_FreeBlockTest_Enable;
	extern Arena OoT_gSystemArena;
	extern OSPifRam __osPifInternalBuff;
	extern u8 __osContLastPoll;
	extern u8 __osMaxControllers;
	extern __OSInode __osPfsInodeCache;
	extern OSPifRam gPifMempakBuf;
	extern u16 gZBuffer[SCREEN_HEIGHT][SCREEN_WIDTH]; // 0x25800 bytes
	extern u64 gGfxSPTaskOutputBuffer[0x3000]; // 0x18000 bytes
	extern u8 OoT_gGfxSPTaskYieldBuffer[OS_YIELD_DATA_SIZE]; // 0xC00 bytes
	extern u8 gGfxSPTaskStack[0x400]; // 0x400 bytes
	extern GfxPool OoT_gGfxPools[2]; // 0x24820 bytes
	extern u8* OoT_gAudioHeap;
	extern u8* OoT_gSystemHeap;
	extern GameState* OoT_gGameState;

#ifdef __cplusplus
};
#endif

#endif
