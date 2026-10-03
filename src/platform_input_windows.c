#include "platform_input.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int platformInputInitialize(FhMod* mod, const FhModHost* host) {
  (void)mod;
  (void)host;
  return 1;
}

void platformInputShutdown(void) {
}

int platformInputActive(void) {
  HWND window = GetForegroundWindow();
  DWORD processId = 0;

  if (window == NULL) return 0;
  GetWindowThreadProcessId(window, &processId);
  return processId == GetCurrentProcessId();
}

int platformKeyDown(PlatformKey key) {
  int virtualKey;

  switch (key) {
    case PLATFORM_KEY_F9:
      virtualKey = VK_F9;
      break;
    case PLATFORM_KEY_F10:
      virtualKey = VK_F10;
      break;
    default:
      return 0;
  }
  return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}
