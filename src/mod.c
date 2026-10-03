#include "cutscene_skip.h"
#include "platform_input.h"

#include <stdarg.h>
#include <stdio.h>

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

FH_MOD_EXPORT int fh_mod_initialize(FhMod* mod, const FhModHost* host) {
  if (!host || host->abiVersion != FH_MOD_ABI_VERSION || host->structSize < sizeof(FhModHost)) return FH_MOD_ERROR;
  if (!host->log || !host->symbolAddress || !host->hookInstall || !host->hookRemove) return FH_MOD_ERROR;
  H = host;
  M = mod;
  if (!platformInputInitialize(mod, host)) {
    modLog(FH_LOG_ERROR, "Cutscene Skip disabled: keyboard input is unavailable");
    return FH_MOD_ERROR;
  }
  if (!cutsceneHooksInstall(mod, host)) {
    cutsceneHooksRemove(mod, host);
    platformInputShutdown();
    modLog(FH_LOG_ERROR, "Cutscene Skip disabled: required host symbols or hooks are unavailable");
    return FH_MOD_ERROR;
  }
  modLog(FH_LOG_INFO, "Cutscene Skip v1.1.0 loaded");
  return FH_MOD_OK;
}

FH_MOD_EXPORT void fh_mod_update(FhMod* mod) {
  int focused = platformInputActive();
  int f9 = focused && platformKeyDown(PLATFORM_KEY_F9);
  int f10 = focused && platformKeyDown(PLATFORM_KEY_F10);
  (void)mod;

  cutsceneSkipUpdate(f9 && !f9Down, f10 && !f10Down);
  f9Down = f9;
  f10Down = f10;
}

FH_MOD_EXPORT void fh_mod_shutdown(FhMod* mod) {
  (void)mod;
  if (H && M) cutsceneHooksRemove(M, H);
  cutsceneSkipReset();
  platformInputShutdown();
  f9Down = 0;
  f10Down = 0;
  H = 0;
  M = 0;
}
