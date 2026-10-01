#include "cutscene_skip.h"

#include <string.h>

#define FH_SKIP_FIRST_RUNTIME_SLOT    0x19
#define FH_SKIP_SLOT_LIMIT            0x55
#define FH_SKIP_MAX_ACTORS            32
#define FH_SKIP_CONDITION_SLOTS       10
#define FH_SKIP_COND_BUTTON_A         0x12
#define FH_SKIP_COND_BUTTON_B         0x13
#define FH_SKIP_COND_DIALOGUE_CLOSED  0x1a
#define FH_SKIP_STEPS_PER_FRAME       120
#define FH_SKIP_STALLED_STEPS_PER_FRAME 8
#define FH_SKIP_STALL_FRAMES          180
#define FH_SKIP_MAX_TOTAL_STEPS       36000
#define FH_SKIP_REQUEST_MAX_AGE       4
#define FH_SKIP_UI_GAMEPLAY           1
#define FH_SKIP_GAME_STATE_RUNNING    1
#define FH_SKIP_RUNSTATE_RUNNING      1
#define FH_SKIP_INACTIVE_FLAGS        (OBJ_FLAG_FREED | OBJ_FLAG_UPDATE_DISABLED)
#define FH_SKIP_TEXT_OP_FIRST         1
#define FH_SKIP_TEXT_OP_CALLBACK      9
#define FH_SKIP_TEXT_OP_LAST_DRAW     14

typedef struct FhSkipActor {
  GameObject* obj;
  int16_t curFrame;
  uint8_t runState;
} FhSkipActor;

unsigned char gSkipWorldEffect;
unsigned char gSkipMuteSfx;
unsigned char gSkipHideSubtitles;
unsigned char gSkipEndSubtitles;
unsigned char gSkipStepping;

static int sAutoSkip;
static int sAutoHoldSlot = -1;
static int16_t sAutoHoldSeqValue;
static int sAutoHoldSawWait;
static uint32_t sFrameCounter;
static int sRequestPending;
static uint32_t sRequestFrame;
static int sInBatch;

static int sActiveSlot = -1;
static int16_t sActiveSeqValue;
static int sTotalSteps;
static int sStallFrames;

static int sSubtitleSlot = -1;
static int16_t sSubtitleSeqValue;

static ObjSeqState* seq_state(GameObject* obj) {
  return (ObjSeqState*)obj->extra;
}

static int is_live_seq_object(GameObject* obj) {
  return obj->classId == SEQ_CLASS_ID && obj->extra != NULL && obj->placementData != NULL &&
         (obj->objectFlags & FH_SKIP_INACTIVE_FLAGS) == 0;
}

static int is_runtime_slot(int slot) {
  return slot >= FH_SKIP_FIRST_RUNTIME_SLOT && slot < FH_SKIP_SLOT_LIMIT && game.objSeqSlotSeqIdTable[slot] != 0;
}

static void advance_slot_frame(int slot) {
  if (slot < 0 || slot >= OBJSEQ_SLOT_COUNT) {
    return;
  }
  game.objSeqSlotPendingFrames[slot] = 0;
  if (game.objSeqSlotResults[slot] != 0 && game.objSeqSlotPrevResults[slot] == 0) {
    game.objSeqSlotPendingFrames[slot] = (int8_t)*game.framesThisStep;
  }
  game.objSeqSlotPrevResults[slot] = game.objSeqSlotResults[slot];
  game.objSeqSlotResults[slot] = 0;
  game.objSeqSlotStreamTimeTable[slot] = game.objSeqSlotDistances[slot];
  game.objSeqSlotDistances[slot] = -1.0f;
  game.objSeqSlotMarks[slot] = game.objSeqSlotMarks[slot] == 2 ? 1 : 0;
}

static uint32_t* find_stream_ids(ObjSeqStreamMapEntry* entries, int trackId) {
  int i;

  for (i = 0; i < OBJSEQ_STREAM_MAP_COUNT; i++) {
    if (entries[i].trackId == trackId) {
      return entries[i].streamIds;
    }
  }
  return NULL;
}

static void release_slot_stream(int slot) {
  int trackId;
  int current;
  int owned;

  if (slot < 0 || slot >= OBJSEQ_SLOT_COUNT || game.objSeqSlotSeqIdTable[slot] == 0) {
    return;
  }
  trackId = (game.objSeqSlotSeqIdTable[slot] - 1) & 0x3fff;
  if (slot == *game.objSeqPreparingStreamSlot) {
    game.AudioStream_CancelPrepared();
    *game.objSeqPreparingStreamSlot = -1;
    *game.objSeqStreamStopped = 0;
    *game.objSeqSubtitleId = -1;
    if (*game.objSeqDeferredTaskTextId != -1) {
      game.gameTextLoadTaskText(*game.objSeqDeferredTaskTextId);
      *game.objSeqDeferredTaskTextId = -1;
      *game.objSeqTaskTextId = -1;
    }
    return;
  }
  current = game.AudioStream_GetCurrentId();
  if (current <= 0 || current > *game.streamsCount || *game.streamsData == NULL) {
    return;
  }
  owned = (*game.streamsData)[current - 1].id == trackId;
  if (owned == 0 && *game.objSeqCurrentTrackId == (uint32_t)trackId &&
      find_stream_ids(game.objSeqStreamTableA, trackId) != NULL) {
    owned = 1;
  }
  if (owned == 0) {
    return;
  }
  game.AudioStream_StopCurrent();
  game.AISetStreamPlayState(AI_STREAM_STOP);
}

static int collect_slot_actors(int slot, FhSkipActor* out, int max) {
  uintptr_t cur = game.objUpdateList->head;
  int linkOffset = game.objUpdateList->nextOffset;
  int count = 0;

  while (cur != 0 && count < max) {
    GameObject* obj = (GameObject*)cur;

    if (is_live_seq_object(obj) && seq_state(obj)->slot == slot) {
      out[count].obj = obj;
      out[count].curFrame = seq_state(obj)->curFrame;
      out[count].runState = seq_state(obj)->runState;
      count++;
    }
    cur = *(uintptr_t*)((uint8_t*)cur + linkOffset);
  }
  return count;
}

static int find_cutscene_slot(GameObject* player) {
  uint8_t scores[FH_SKIP_SLOT_LIMIT];
  uintptr_t cur = game.objUpdateList->head;
  int linkOffset = game.objUpdateList->nextOffset;
  int best = -1;
  int bestScore = 0;
  int slot;

  memset(scores, 0, sizeof(scores));
  while (cur != 0) {
    GameObject* obj = (GameObject*)cur;

    if (is_live_seq_object(obj)) {
      ObjSeqState* state = seq_state(obj);

      slot = state->slot;
      if (is_runtime_slot(slot) && state->runState != 0) {
        if (state->isCameraSeq != 0) {
          scores[slot] |= 2;
        }
        if (player != NULL && state->targetObj == player) {
          scores[slot] |= 1;
        }
      }
    }
    cur = *(uintptr_t*)((uint8_t*)cur + linkOffset);
  }

  for (slot = FH_SKIP_FIRST_RUNTIME_SLOT; slot < FH_SKIP_SLOT_LIMIT; slot++) {
    int score = scores[slot];

    if (score == 0) {
      continue;
    }
    if (slot == *game.curSeqNo) {
      score |= 4;
    }
    if (score > bestScore) {
      best = slot;
      bestScore = score;
    }
  }
  return best;
}

static int gameplay_blocked(void) {
  return game.getGameState() != FH_SKIP_GAME_STATE_RUNNING || game.getCurUiDll() != FH_SKIP_UI_GAMEPLAY ||
         *game.timeStop != 0 || *game.warpRequested != 0 || *game.gameLoopReloadRequested != 0 ||
         *game.gameLoopMapLoadPending != 0 || game.Obj_GetPlayerObject() == NULL;
}

static int actors_waiting(const FhSkipActor* actors, int count) {
  int i;
  int k;

  if (game.isTalkingToNpc() != 0) {
    return 1;
  }
  for (i = 0; i < count; i++) {
    ObjSeqState* state;

    if ((actors[i].obj->objectFlags & FH_SKIP_INACTIVE_FLAGS) != 0) {
      continue;
    }
    state = seq_state(actors[i].obj);
    for (k = 0; k < FH_SKIP_CONDITION_SLOTS; k++) {
      uint8_t op = state->conditionOpcodes[k];

      if (op == FH_SKIP_COND_BUTTON_A || op == FH_SKIP_COND_BUTTON_B || op == FH_SKIP_COND_DIALOGUE_CLOSED) {
        return 1;
      }
    }
  }
  return 0;
}

static void subtitle_cleanup_update(void) {
  if (sSubtitleSlot < 0) {
    return;
  }
  if (gSkipHideSubtitles == 0) {
    sSubtitleSlot = -1;
    return;
  }
  if (game.objSeqSlotSeqIdTable[sSubtitleSlot] != sSubtitleSeqValue) {
    gSkipEndSubtitles = 1;
    sSubtitleSlot = -1;
  }
}

static void end_skip(void) {
  *game.objSeqStreamSuppressed = 0;
  gSkipMuteSfx = 0;
  sActiveSlot = -1;
}

static void finish_completed(void) {
  end_skip();
  subtitle_cleanup_update();
}

static void stop_early(void) {
  sAutoHoldSlot = sActiveSlot;
  sAutoHoldSeqValue = sActiveSeqValue;
  sAutoHoldSawWait = 0;
  end_skip();
}

static int text_commands_are_draw_only(int first, int end) {
  GameTextSlot* slots = *game.gameTextCommandSlots;
  int i;

  for (i = first; i < end; i++) {
    int op = slots[i].opcode;

    if (op < FH_SKIP_TEXT_OP_FIRST || op > FH_SKIP_TEXT_OP_LAST_DRAW || op == FH_SKIP_TEXT_OP_CALLBACK) {
      return 0;
    }
  }
  return 1;
}

static int step_slot(int slot, const FhSkipActor* actors, int count) {
  uint8_t savedFrames = *game.framesThisStep;
  uint8_t savedUnclamped = *game.framesThisStepUnclamped;
  float savedDelta = *game.timeDelta;
  float savedInverse = *game.oneOverTimeDelta;
  int savedTextCount = *game.gameTextCommandCount;
  char* savedTextCursor = *game.gameTextCommandStringCursor;
  int keptText = 0;
  int i;

  *game.framesThisStep = 1;
  *game.framesThisStepUnclamped = 1;
  *game.timeDelta = 1.0f;
  *game.oneOverTimeDelta = 1.0f;
  gSkipStepping = 1;
  for (i = 0; i < count; i++) {
    if ((actors[i].obj->objectFlags & FH_SKIP_INACTIVE_FLAGS) == 0) {
      game.Obj_UpdateObject(actors[i].obj);
    }
  }
  if (game.objSeqSlotSeqIdTable[slot] == sActiveSeqValue) {
    advance_slot_frame(slot);
  }
  gSkipStepping = 0;
  *game.framesThisStep = savedFrames;
  *game.framesThisStepUnclamped = savedUnclamped;
  *game.timeDelta = savedDelta;
  *game.oneOverTimeDelta = savedInverse;
  if (*game.gameTextCommandCount > savedTextCount) {
    if (text_commands_are_draw_only(savedTextCount, *game.gameTextCommandCount)) {
      *game.gameTextCommandCount = savedTextCount;
      *game.gameTextCommandStringCursor = savedTextCursor;
    } else {
      keptText = 1;
    }
  }
  return keptText;
}

static void run_batch(void) {
  FhSkipActor actors[FH_SKIP_MAX_ACTORS];
  int count;
  int steps = 0;
  int stalledSteps = 0;
  int i;

  while (steps < FH_SKIP_STEPS_PER_FRAME) {
    int progressed = 0;
    int looped = 0;

    if (game.objSeqSlotSeqIdTable[sActiveSlot] != sActiveSeqValue) {
      finish_completed();
      return;
    }
    if (gameplay_blocked()) {
      stop_early();
      return;
    }
    count = collect_slot_actors(sActiveSlot, actors, FH_SKIP_MAX_ACTORS);
    if (count == 0) {
      finish_completed();
      return;
    }
    if (actors_waiting(actors, count) || sTotalSteps >= FH_SKIP_MAX_TOTAL_STEPS) {
      stop_early();
      return;
    }

    gSkipWorldEffect = 0;
    if (step_slot(sActiveSlot, actors, count) != 0) {
      gSkipWorldEffect = 1;
    }
    steps++;
    sTotalSteps++;

    if (game.objSeqSlotSeqIdTable[sActiveSlot] != sActiveSeqValue) {
      finish_completed();
      return;
    }
    for (i = 0; i < count; i++) {
      ObjSeqState* state;

      if ((actors[i].obj->objectFlags & FH_SKIP_INACTIVE_FLAGS) != 0) {
        progressed = 1;
        continue;
      }
      state = seq_state(actors[i].obj);
      if (state->runState != actors[i].runState || state->curFrame != actors[i].curFrame) {
        progressed = 1;
      }
      if (actors[i].runState == FH_SKIP_RUNSTATE_RUNNING && state->runState == FH_SKIP_RUNSTATE_RUNNING &&
          actors[i].curFrame - state->curFrame > 1) {
        looped = 1;
      }
    }
    if (looped != 0) {
      stop_early();
      return;
    }
    if (progressed != 0) {
      sStallFrames = 0;
      stalledSteps = 0;
    } else if (++stalledSteps >= FH_SKIP_STALLED_STEPS_PER_FRAME) {
      if (++sStallFrames >= FH_SKIP_STALL_FRAMES) {
        stop_early();
      }
      return;
    }
    if (gSkipWorldEffect != 0) {
      return;
    }
  }
}

static void try_accept(void) {
  FhSkipActor actors[FH_SKIP_MAX_ACTORS];
  int slot;
  int count;

  if (gameplay_blocked()) {
    return;
  }
  slot = find_cutscene_slot(game.Obj_GetPlayerObject());
  if (slot < 0) {
    return;
  }
  count = collect_slot_actors(slot, actors, FH_SKIP_MAX_ACTORS);
  if (actors_waiting(actors, count)) {
    return;
  }

  sActiveSlot = slot;
  sActiveSeqValue = game.objSeqSlotSeqIdTable[slot];
  sTotalSteps = 0;
  sStallFrames = 0;
  cutsceneNotify("Cutscene Skipped");

  release_slot_stream(slot);
  *game.objSeqStreamSuppressed = 1;
  gSkipMuteSfx = 1;
  if (gSkipSubtitlesAvailable != 0 && *game.subtitleActive != 0) {
    gSkipHideSubtitles = 1;
    gSkipEndSubtitles = 0;
    sSubtitleSlot = slot;
    sSubtitleSeqValue = sActiveSeqValue;
  }
}

void cutsceneSkipUpdate(int skipPressed, int autoPressed) {
  sFrameCounter++;
  if (skipPressed != 0) {
    if (sActiveSlot < 0 && !gameplay_blocked()) {
      sRequestPending = 1;
      sRequestFrame = sFrameCounter;
    }
  }

  if (autoPressed != 0) {
    sAutoSkip = !sAutoSkip;
    sAutoHoldSlot = -1;
    cutsceneNotify(sAutoSkip ? "Cutscene Skip Enabled" : "Cutscene Skip Disabled");
  }

  subtitle_cleanup_update();
}

static int auto_skip_ready(void) {
  FhSkipActor actors[FH_SKIP_MAX_ACTORS];
  int slot;
  int count;
  int waiting;

  if (gameplay_blocked()) {
    sAutoHoldSawWait = 1;
    return 0;
  }
  slot = find_cutscene_slot(game.Obj_GetPlayerObject());
  if (slot < 0) {
    return 0;
  }
  count = collect_slot_actors(slot, actors, FH_SKIP_MAX_ACTORS);
  waiting = actors_waiting(actors, count);
  if (slot == sAutoHoldSlot && game.objSeqSlotSeqIdTable[slot] == sAutoHoldSeqValue) {
    if (waiting) {
      sAutoHoldSawWait = 1;
      return 0;
    }
    if (sAutoHoldSawWait == 0) {
      return 0;
    }
    sAutoHoldSlot = -1;
  }
  return !waiting;
}

void cutsceneSkipRunFrame(void) {
  if (sInBatch != 0) {
    return;
  }
  sInBatch = 1;
  if (sAutoSkip != 0 && sActiveSlot < 0 && sRequestPending == 0 && auto_skip_ready() != 0) {
    sRequestPending = 1;
    sRequestFrame = sFrameCounter;
  }
  if (sRequestPending != 0) {
    sRequestPending = 0;
    if (sFrameCounter - sRequestFrame <= FH_SKIP_REQUEST_MAX_AGE) {
      try_accept();
    }
  }
  if (sActiveSlot >= 0) {
    run_batch();
  }
  sInBatch = 0;
}

void cutsceneSkipReset(void) {
  if (sActiveSlot >= 0 && game.objSeqStreamSuppressed != NULL) {
    *game.objSeqStreamSuppressed = 0;
  }
  gSkipWorldEffect = 0;
  gSkipMuteSfx = 0;
  gSkipHideSubtitles = 0;
  gSkipEndSubtitles = 0;
  gSkipStepping = 0;
  sAutoSkip = 0;
  sAutoHoldSlot = -1;
  sAutoHoldSeqValue = 0;
  sAutoHoldSawWait = 0;
  sFrameCounter = 0;
  sRequestPending = 0;
  sRequestFrame = 0;
  sInBatch = 0;
  sActiveSlot = -1;
  sActiveSeqValue = 0;
  sTotalSteps = 0;
  sStallFrames = 0;
  sSubtitleSlot = -1;
  sSubtitleSeqValue = 0;
}
