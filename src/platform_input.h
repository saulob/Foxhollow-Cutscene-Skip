#ifndef PLATFORM_INPUT_H_
#define PLATFORM_INPUT_H_

#include "foxhollow_mod_api.h"

typedef enum PlatformKey {
  PLATFORM_KEY_F9,
  PLATFORM_KEY_F10
} PlatformKey;

int platformInputInitialize(FhMod* mod, const FhModHost* host);
void platformInputShutdown(void);
int platformInputActive(void);
int platformKeyDown(PlatformKey key);

#endif
