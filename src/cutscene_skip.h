#ifndef CUTSCENE_SKIP_H_
#define CUTSCENE_SKIP_H_

#include <stddef.h>
#include <stdint.h>

#include "foxhollow_mod_api.h"

#define SEQ_CLASS_ID 0x10
#define OBJ_FLAG_FREED 0x40
#define OBJ_FLAG_UPDATE_DISABLED 0x8000
#define OBJSEQ_SLOT_COUNT 85
#define OBJSEQ_STREAM_MAP_COUNT 5
#define AI_STREAM_STOP 0

typedef struct GameObject {
  uint8_t pad00[0x50];
  int16_t classId;
  uint8_t pad52[0x06];
  void* placementData;
  uint8_t pad60[0x98];
  uint16_t objectFlags;
  uint8_t padFA[0x06];
  void* extra;
  void* animEventCallback;
} GameObject;

typedef struct ObjSeqState {
  void* targetObj;
  uint8_t pad08[0x57];
  int8_t slot;
  int16_t curFrame;
  uint8_t pad62[0x21];
  int8_t isCameraSeq;
  uint8_t pad84[0x02];
  uint8_t runState;
  uint8_t pad87[0x0C];
  uint8_t eventCount;
  uint8_t pad94[0xBC];
  uint8_t conditionOpcodes[10];
} ObjSeqState;

typedef struct ObjLinkedList {
  int16_t count;
  int16_t nextOffset;
  uintptr_t head;
} ObjLinkedList;

typedef struct GameTextSlot {
  int32_t opcode;
  uint8_t pad04[0x1C];
} GameTextSlot;

typedef struct StreamEntry {
  uint16_t id;
  uint8_t pad02[0x14];
} StreamEntry;

typedef struct ObjSeqStreamMapEntry {
  int32_t trackId;
  uint32_t* streamIds;
} ObjSeqStreamMapEntry;

_Static_assert(offsetof(GameObject, classId) == 0x50, "GameObject.classId");
_Static_assert(offsetof(GameObject, placementData) == 0x58, "GameObject.placementData");
_Static_assert(offsetof(GameObject, objectFlags) == 0xF8, "GameObject.objectFlags");
_Static_assert(offsetof(GameObject, extra) == 0x100, "GameObject.extra");
_Static_assert(offsetof(GameObject, animEventCallback) == 0x108, "GameObject.animEventCallback");
_Static_assert(offsetof(ObjSeqState, slot) == 0x5F, "ObjSeqState.slot");
_Static_assert(offsetof(ObjSeqState, curFrame) == 0x60, "ObjSeqState.curFrame");
_Static_assert(offsetof(ObjSeqState, isCameraSeq) == 0x83, "ObjSeqState.isCameraSeq");
_Static_assert(offsetof(ObjSeqState, runState) == 0x86, "ObjSeqState.runState");
_Static_assert(offsetof(ObjSeqState, eventCount) == 0x93, "ObjSeqState.eventCount");
_Static_assert(offsetof(ObjSeqState, conditionOpcodes) == 0x150, "ObjSeqState.conditionOpcodes");
_Static_assert(offsetof(ObjLinkedList, nextOffset) == 0x02, "ObjLinkedList.nextOffset");
_Static_assert(offsetof(ObjLinkedList, head) == 0x08, "ObjLinkedList.head");
_Static_assert(sizeof(GameTextSlot) == 0x20, "GameTextSlot");
_Static_assert(sizeof(StreamEntry) == 0x16, "StreamEntry");
_Static_assert(sizeof(ObjSeqStreamMapEntry) == 0x10, "ObjSeqStreamMapEntry");

typedef struct CutsceneGame {
  int (*getGameState)(void);
  int (*getCurUiDll)(void);
  GameObject* (*Obj_GetPlayerObject)(void);
  int32_t (*isTalkingToNpc)(void);
  void (*Obj_UpdateObject)(GameObject* obj);
  void (*AudioStream_CancelPrepared)(void);
  int32_t (*AudioStream_GetCurrentId)(void);
  void (*AudioStream_StopCurrent)(void);
  void (*AISetStreamPlayState)(uint32_t state);
  void (*gameTextLoadTaskText)(int taskId);
  int (*gameTextGetTaskText)(int id, int* outTextSeqId, int* outDirId);

  int16_t* objSeqSlotSeqIdTable;
  int8_t* objSeqSlotPendingFrames;
  int8_t* objSeqSlotResults;
  int8_t* objSeqSlotPrevResults;
  float* objSeqSlotStreamTimeTable;
  float* objSeqSlotDistances;
  uint8_t* objSeqSlotMarks;
  int* objSeqStreamSuppressed;
  int* objSeqPreparingStreamSlot;
  int16_t* objSeqStreamStopped;
  int* objSeqSubtitleId;
  int* objSeqDeferredTaskTextId;
  int* objSeqTaskTextId;
  uint32_t* objSeqCurrentTrackId;
  ObjSeqStreamMapEntry* objSeqStreamTableA;
  int8_t* objSeqDeferredCmdCount;
  uint8_t* curSeqNo;
  uint8_t* timeStop;
  uint8_t* warpRequested;
  uint8_t* gameLoopReloadRequested;
  uint8_t* gameLoopMapLoadPending;
  ObjLinkedList* objUpdateList;
  uint8_t* framesThisStep;
  uint8_t* framesThisStepUnclamped;
  float* timeDelta;
  float* oneOverTimeDelta;
  int* gameTextCommandCount;
  char** gameTextCommandStringCursor;
  GameTextSlot* const* gameTextCommandSlots;
  StreamEntry** streamsData;
  int* streamsCount;
  int* subtitleActive;
  int* subtitlesEnabled;
  int16_t* gameTextTaskTextAllowList;
} CutsceneGame;

extern CutsceneGame game;

extern unsigned char gSkipWorldEffect;
extern unsigned char gSkipMuteSfx;
extern unsigned char gSkipHideSubtitles;
extern unsigned char gSkipEndSubtitles;
extern unsigned char gSkipStepping;
extern int gSkipSubtitlesAvailable;

void modLog(FhLogLevel level, const char* format, ...);
void cutsceneNotify(const char* text);

int cutsceneHooksInstall(FhMod* mod, const FhModHost* host);
void cutsceneHooksRemove(FhMod* mod, const FhModHost* host);

void cutsceneSkipUpdate(int skipPressed, int autoPressed);
void cutsceneSkipRunFrame(void);
void cutsceneSkipReset(void);

#endif
