#define _GNU_SOURCE

#include "platform_input.h"
#include "cutscene_skip.h"

#include <dlfcn.h>
#include <stdbool.h>

#define SDL3_SCANCODE_F9 66
#define SDL3_SCANCODE_F10 67

typedef const bool* (*SdlGetKeyboardStateFn)(int* numkeys);
typedef void* (*SdlGetKeyboardFocusFn)(void);

static SdlGetKeyboardStateFn sGetKeyboardState;
static SdlGetKeyboardFocusFn sGetKeyboardFocus;

static void* resolve_sdl_symbol(FhMod* mod, const FhModHost* host, const char* name) {
  void* address = dlsym(RTLD_DEFAULT, name);

  if (address == NULL) {
    address = host->symbolAddress(mod, name);
  }
  if (address == NULL) {
    modLog(FH_LOG_ERROR, "could not resolve %s", name);
  }
  return address;
}

int platformInputInitialize(FhMod* mod, const FhModHost* host) {
  const bool* keys;
  int count = 0;

  sGetKeyboardState = (SdlGetKeyboardStateFn)resolve_sdl_symbol(mod, host, "SDL_GetKeyboardState");
  sGetKeyboardFocus = (SdlGetKeyboardFocusFn)resolve_sdl_symbol(mod, host, "SDL_GetKeyboardFocus");
  if (sGetKeyboardState == NULL || sGetKeyboardFocus == NULL) {
    platformInputShutdown();
    return 0;
  }
  keys = sGetKeyboardState(&count);
  if (keys == NULL || count <= SDL3_SCANCODE_F10) {
    modLog(FH_LOG_ERROR, "SDL keyboard state is unavailable");
    platformInputShutdown();
    return 0;
  }
  return 1;
}

void platformInputShutdown(void) {
  sGetKeyboardState = NULL;
  sGetKeyboardFocus = NULL;
}

int platformInputActive(void) {
  return sGetKeyboardFocus != NULL && sGetKeyboardFocus() != NULL;
}

int platformKeyDown(PlatformKey key) {
  const bool* keys;
  int count = 0;
  int scancode;

  switch (key) {
    case PLATFORM_KEY_F9:
      scancode = SDL3_SCANCODE_F9;
      break;
    case PLATFORM_KEY_F10:
      scancode = SDL3_SCANCODE_F10;
      break;
    default:
      return 0;
  }
  if (sGetKeyboardState == NULL) return 0;
  keys = sGetKeyboardState(&count);
  return keys != NULL && scancode < count && keys[scancode];
}
