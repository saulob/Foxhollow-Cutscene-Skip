#include "cutscene_skip.h"

#define SEQ_OP_SEND_MESSAGE 7
#define SEQ_OP_SET 2
#define SEQ_SET_EVENT 0
#define SEQ_SET_GAMEBIT 6
#define CMD06_PLAY_STREAM 40
#define TASK_TEXT_ALLOW_COUNT 0xb

typedef struct Symbol {
  const char* name;
  void** address;
} Symbol;

typedef void (*HookFn)(void);

typedef struct Hook {
  const char* name;
  HookFn replacement;
  void** original;
  void* target;
} Hook;

CutsceneGame game;
int gSkipSubtitlesAvailable;

static int sInSubtitleDraw;

static void (*origRunBgCmds)(void);
static int (*origExecCmd06)(GameObject*, GameObject*, uint8_t*, int, int8_t);
static int (*origSubCmd0B)(GameObject*, GameObject*, uint8_t*, uint8_t*, int16_t, int16_t, int8_t, int8_t);
static void (*origCallSeqFn)(GameObject*, GameObject*, ObjSeqState*, int);
static void (*origBgCmds0D)(uint8_t*, GameObject*, int);
static void (*origEndObjSequence)(int);
static int (*origObjSeqStart)(int, GameObject*, int);
static void (*origSfxPlayFromObjectEx)(GameObject*, void*, uint32_t, uint16_t);
static void (*origSubtitleUpdateAndDraw)(int);
static void (*origSubtitleStop)(void);
static void (*origSubtitleStart)(int);
static void (*origGameTextLoadTaskText)(int);
static void (*origGameTextSetColor)(uint8_t, uint8_t, uint8_t, uint8_t);
static void (*origGameTextShowStr)(char*, int, int, int);

static void hookRunBgCmds(void) {
  origRunBgCmds();
  cutsceneSkipRunFrame();
}

static int hookExecCmd06(GameObject* obj, GameObject* sourceObj, uint8_t* seq, int cmd, int8_t flag) {
  gSkipWorldEffect = 1;
  if ((cmd & 0xff) == CMD06_PLAY_STREAM && *game.objSeqStreamSuppressed != 0) {
    return 1;
  }
  return origExecCmd06(obj, sourceObj, seq, cmd, flag);
}

static int hookSubCmd0B(GameObject* obj, GameObject* sourceObj, uint8_t* seq, uint8_t* cmds, int16_t xrot,
                        int16_t count, int8_t flag1, int8_t flag2) {
  int i;

  if (gSkipStepping != 0) {
    for (i = 0; i < count; i++) {
      const uint8_t* cmd = cmds + i * 4;
      uint32_t packed = ((uint32_t)cmd[0] << 24) | ((uint32_t)cmd[1] << 16) | ((uint32_t)cmd[2] << 8) | cmd[3];
      int opcode = packed & 0x3f;
      int operand = (packed >> 6) & 0x3ff;

      if (opcode == SEQ_OP_SEND_MESSAGE && sourceObj != obj) {
        gSkipWorldEffect = 1;
      } else if (opcode == SEQ_OP_SET && flag1 == 0 && (operand == SEQ_SET_EVENT || operand == SEQ_SET_GAMEBIT)) {
        gSkipWorldEffect = 1;
      }
    }
  }
  return origSubCmd0B(obj, sourceObj, seq, cmds, xrot, count, flag1, flag2);
}

static void hookCallSeqFn(GameObject* obj, GameObject* sourceObj, ObjSeqState* seq, int action) {
  if (obj->animEventCallback != NULL && seq->eventCount != 0) {
    gSkipWorldEffect = 1;
  }
  origCallSeqFn(obj, sourceObj, seq, action);
}

static void hookBgCmds0D(uint8_t* seq, GameObject* obj, int skipSpawns) {
  if (*game.objSeqDeferredCmdCount > 0) {
    gSkipWorldEffect = 1;
  }
  origBgCmds0D(seq, obj, skipSpawns);
}

static void hookEndObjSequence(int seq) {
  gSkipWorldEffect = 1;
  origEndObjSequence(seq);
}

static int hookObjSeqStart(int seqIdx, GameObject* obj, int flags) {
  gSkipWorldEffect = 1;
  return origObjSeqStart(seqIdx, obj, flags);
}

static void hookSfxPlayFromObjectEx(GameObject* obj, void* pos, uint32_t channel, uint16_t sfxId) {
  if (gSkipMuteSfx != 0) {
    return;
  }
  origSfxPlayFromObjectEx(obj, pos, channel, sfxId);
}

static void hookSubtitleStop(void) {
  gSkipHideSubtitles = 0;
  gSkipEndSubtitles = 0;
  origSubtitleStop();
}

static void hookSubtitleUpdateAndDraw(int unused) {
  if (gSkipEndSubtitles != 0) {
    hookSubtitleStop();
  }
  sInSubtitleDraw = 1;
  origSubtitleUpdateAndDraw(unused);
  sInSubtitleDraw = 0;
  if (*game.subtitleActive == 0) {
    gSkipHideSubtitles = 0;
    gSkipEndSubtitles = 0;
  }
}

static void hookSubtitleStart(int x) {
  if (*game.subtitlesEnabled != 0) {
    gSkipHideSubtitles = 0;
    gSkipEndSubtitles = 0;
  }
  origSubtitleStart(x);
}

static int task_text_allowed(int taskId) {
  int i;

  for (i = 0; i < TASK_TEXT_ALLOW_COUNT; i++) {
    if (taskId == game.gameTextTaskTextAllowList[i]) {
      return 1;
    }
  }
  return 0;
}

static void hookGameTextLoadTaskText(int taskId) {
  int textId;
  int dirId;

  if (game.gameTextGetTaskText(taskId, &textId, &dirId) != 0 &&
      (*game.subtitlesEnabled != 0 || task_text_allowed(taskId))) {
    gSkipHideSubtitles = 0;
    gSkipEndSubtitles = 0;
  }
  origGameTextLoadTaskText(taskId);
}

static void hookGameTextSetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (sInSubtitleDraw != 0 && gSkipHideSubtitles != 0) {
    return;
  }
  origGameTextSetColor(r, g, b, a);
}

static void hookGameTextShowStr(char* text, int box, int cursorX, int cursorY) {
  if (sInSubtitleDraw != 0 && gSkipHideSubtitles != 0) {
    return;
  }
  origGameTextShowStr(text, box, cursorX, cursorY);
}

static const Symbol kRequiredSymbols[] = {
    {"getGameState", (void**)&game.getGameState},
    {"getCurUiDll", (void**)&game.getCurUiDll},
    {"Obj_GetPlayerObject", (void**)&game.Obj_GetPlayerObject},
    {"isTalkingToNpc", (void**)&game.isTalkingToNpc},
    {"Obj_UpdateObject", (void**)&game.Obj_UpdateObject},
    {"AudioStream_CancelPrepared", (void**)&game.AudioStream_CancelPrepared},
    {"AudioStream_GetCurrentId", (void**)&game.AudioStream_GetCurrentId},
    {"AudioStream_StopCurrent", (void**)&game.AudioStream_StopCurrent},
    {"AISetStreamPlayState", (void**)&game.AISetStreamPlayState},
    {"gameTextLoadTaskText", (void**)&game.gameTextLoadTaskText},
    {"gObjSeqSlotSeqIdTable", (void**)&game.objSeqSlotSeqIdTable},
    {"gObjSeqSlotPendingFrames", (void**)&game.objSeqSlotPendingFrames},
    {"gObjSeqSlotResults", (void**)&game.objSeqSlotResults},
    {"gObjSeqSlotPrevResults", (void**)&game.objSeqSlotPrevResults},
    {"gObjSeqSlotStreamTimeTable", (void**)&game.objSeqSlotStreamTimeTable},
    {"gObjSeqSlotDistances", (void**)&game.objSeqSlotDistances},
    {"gObjSeqSlotMarks", (void**)&game.objSeqSlotMarks},
    {"gObjSeqStreamSuppressed", (void**)&game.objSeqStreamSuppressed},
    {"gObjSeqPreparingStreamSlot", (void**)&game.objSeqPreparingStreamSlot},
    {"gObjSeqStreamStopped", (void**)&game.objSeqStreamStopped},
    {"gObjSeqSubtitleId", (void**)&game.objSeqSubtitleId},
    {"gObjSeqDeferredTaskTextId", (void**)&game.objSeqDeferredTaskTextId},
    {"gObjSeqTaskTextId", (void**)&game.objSeqTaskTextId},
    {"gObjSeqCurrentTrackId", (void**)&game.objSeqCurrentTrackId},
    {"gObjSeqStreamTableA", (void**)&game.objSeqStreamTableA},
    {"gObjSeqDeferredCmdCount", (void**)&game.objSeqDeferredCmdCount},
    {"curSeqNo", (void**)&game.curSeqNo},
    {"timeStop", (void**)&game.timeStop},
    {"gWarpRequested", (void**)&game.warpRequested},
    {"gGameLoopReloadRequested", (void**)&game.gameLoopReloadRequested},
    {"gGameLoopMapLoadPending", (void**)&game.gameLoopMapLoadPending},
    {"gObjUpdateList", (void**)&game.objUpdateList},
    {"framesThisStep", (void**)&game.framesThisStep},
    {"framesThisStepUnclamped", (void**)&game.framesThisStepUnclamped},
    {"timeDelta", (void**)&game.timeDelta},
    {"oneOverTimeDelta", (void**)&game.oneOverTimeDelta},
    {"lbl_803DC9C8", (void**)&game.gameTextCommandCount},
    {"gGameTextCommandStringCursor", (void**)&game.gameTextCommandStringCursor},
    {"gGameTextCommandSlots", (void**)&game.gameTextCommandSlots},
    {"gStreamsData", (void**)&game.streamsData},
    {"gStreamsCount", (void**)&game.streamsCount},
};

static const Symbol kSubtitleSymbols[] = {
    {"gameTextGetTaskText", (void**)&game.gameTextGetTaskText},
    {"gSubtitleActive", (void**)&game.subtitleActive},
    {"gSubtitlesEnabled", (void**)&game.subtitlesEnabled},
    {"gGameTextTaskTextAllowList", (void**)&game.gameTextTaskTextAllowList},
};

static Hook sCoreHooks[] = {
    {"ObjSeq_runBgCmds", (HookFn)hookRunBgCmds, (void**)&origRunBgCmds, NULL},
    {"objSeqExecCmd06", (HookFn)hookExecCmd06, (void**)&origExecCmd06, NULL},
    {"seqDoSubCmd0B", (HookFn)hookSubCmd0B, (void**)&origSubCmd0B, NULL},
    {"objCallSeqFn", (HookFn)hookCallSeqFn, (void**)&origCallSeqFn, NULL},
    {"objSeqDoBgCmds0D", (HookFn)hookBgCmds0D, (void**)&origBgCmds0D, NULL},
    {"endObjSequence", (HookFn)hookEndObjSequence, (void**)&origEndObjSequence, NULL},
    {"ObjSeq_start", (HookFn)hookObjSeqStart, (void**)&origObjSeqStart, NULL},
};

static Hook sSfxHooks[] = {
    {"Sfx_PlayFromObjectEx", (HookFn)hookSfxPlayFromObjectEx, (void**)&origSfxPlayFromObjectEx, NULL},
};

static Hook sSubtitleHooks[] = {
    {"subtitleStop", (HookFn)hookSubtitleStop, (void**)&origSubtitleStop, NULL},
    {"subtitleUpdateAndDraw", (HookFn)hookSubtitleUpdateAndDraw, (void**)&origSubtitleUpdateAndDraw, NULL},
    {"subtitleStart", (HookFn)hookSubtitleStart, (void**)&origSubtitleStart, NULL},
    {"gameTextLoadTaskText", (HookFn)hookGameTextLoadTaskText, (void**)&origGameTextLoadTaskText, NULL},
    {"gameTextSetColor", (HookFn)hookGameTextSetColor, (void**)&origGameTextSetColor, NULL},
    {"gameTextShowStr", (HookFn)hookGameTextShowStr, (void**)&origGameTextShowStr, NULL},
};

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

static int resolve_symbols(FhMod* mod, const FhModHost* host, const Symbol* symbols, int count, FhLogLevel level) {
  int ok = 1;
  int i;

  for (i = 0; i < count; i++) {
    *symbols[i].address = host->symbolAddress(mod, symbols[i].name);
    if (*symbols[i].address == NULL) {
      modLog(level, "could not resolve %s", symbols[i].name);
      ok = 0;
    }
  }
  return ok;
}

static void remove_hooks(FhMod* mod, const FhModHost* host, Hook* hooks, int count) {
  int i;

  for (i = count - 1; i >= 0; i--) {
    if (hooks[i].target != NULL) {
      host->hookRemove(mod, hooks[i].target);
      hooks[i].target = NULL;
      *hooks[i].original = NULL;
    }
  }
}

static int install_hooks(FhMod* mod, const FhModHost* host, Hook* hooks, int count, FhLogLevel level) {
  int i;

  for (i = 0; i < count; i++) {
    void* target = host->symbolAddress(mod, hooks[i].name);

    if (target == NULL) {
      modLog(level, "could not resolve %s", hooks[i].name);
      remove_hooks(mod, host, hooks, count);
      return 0;
    }
    if (host->hookInstall(mod, target, (void*)hooks[i].replacement, hooks[i].original) != FH_MOD_OK) {
      modLog(level, "could not hook %s (no patch pad?)", hooks[i].name);
      remove_hooks(mod, host, hooks, count);
      return 0;
    }
    hooks[i].target = target;
    modLog(FH_LOG_INFO, "hooked %s", hooks[i].name);
  }
  return 1;
}

int cutsceneHooksInstall(FhMod* mod, const FhModHost* host) {
  int subtitleSymbols;

  if (!resolve_symbols(mod, host, kRequiredSymbols, COUNT_OF(kRequiredSymbols), FH_LOG_ERROR)) {
    return 0;
  }
  subtitleSymbols = resolve_symbols(mod, host, kSubtitleSymbols, COUNT_OF(kSubtitleSymbols), FH_LOG_WARN);

  if (!install_hooks(mod, host, sCoreHooks, COUNT_OF(sCoreHooks), FH_LOG_ERROR)) {
    return 0;
  }
  if (!install_hooks(mod, host, sSfxHooks, COUNT_OF(sSfxHooks), FH_LOG_WARN)) {
    modLog(FH_LOG_WARN, "SFX muting during skip unavailable");
  }
  gSkipSubtitlesAvailable =
      subtitleSymbols && install_hooks(mod, host, sSubtitleHooks, COUNT_OF(sSubtitleHooks), FH_LOG_WARN);
  if (!gSkipSubtitlesAvailable) {
    modLog(FH_LOG_WARN, "skipped-sequence subtitle hiding unavailable");
  }
  return 1;
}

void cutsceneHooksRemove(FhMod* mod, const FhModHost* host) {
  remove_hooks(mod, host, sSubtitleHooks, COUNT_OF(sSubtitleHooks));
  remove_hooks(mod, host, sSfxHooks, COUNT_OF(sSfxHooks));
  remove_hooks(mod, host, sCoreHooks, COUNT_OF(sCoreHooks));
  gSkipSubtitlesAvailable = 0;
  sInSubtitleDraw = 0;
}
