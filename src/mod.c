#include "cutscene_skip.h"

#include <stdarg.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static const FhModHost* H;
static FhMod* M;
static int f9Down;
static int f10Down;

void modLog(FhLogLevel level, const char* format, ...) {
  char message[256];
  va_list args;

  if (!H || !H->log || !M) return;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  H->log(M, level, message);
}

void cutsceneNotify(const char* text) {
  modLog(FH_LOG_INFO, "%s", text);
}

static int game_window_focused(void) {
  HWND window = GetForegroundWindow();
  DWORD processId = 0;

  if (window == NULL) return 0;
  GetWindowThreadProcessId(window, &processId);
  return processId == GetCurrentProcessId();
}

static int key_down(int key) {
  return (GetAsyncKeyState(key) & 0x8000) != 0;
}

FH_MOD_EXPORT int fh_mod_initialize(FhMod* mod, const FhModHost* host) {
  if (!host || host->abiVersion != FH_MOD_ABI_VERSION || host->structSize < sizeof(FhModHost)) return FH_MOD_ERROR;
  if (!host->log || !host->symbolAddress || !host->hookInstall || !host->hookRemove) return FH_MOD_ERROR;
  H = host;
  M = mod;
  if (!cutsceneHooksInstall(mod, host)) {
    cutsceneHooksRemove(mod, host);
    modLog(FH_LOG_ERROR, "Cutscene Skip disabled: required host symbols or hooks are unavailable");
    return FH_MOD_ERROR;
  }
  modLog(FH_LOG_INFO, "Cutscene Skip v1.0.0 loaded");
  return FH_MOD_OK;
}

FH_MOD_EXPORT void fh_mod_update(FhMod* mod) {
  int focused = game_window_focused();
  int f9 = focused && key_down(VK_F9);
  int f10 = focused && key_down(VK_F10);
  (void)mod;

  cutsceneSkipUpdate(f9 && !f9Down, f10 && !f10Down);
  f9Down = f9;
  f10Down = f10;
}

FH_MOD_EXPORT void fh_mod_shutdown(FhMod* mod) {
  (void)mod;
  if (H && M) cutsceneHooksRemove(M, H);
  cutsceneSkipReset();
  f9Down = 0;
  f10Down = 0;
  H = 0;
  M = 0;
}
