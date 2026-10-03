# Foxhollow Cutscene Skip

A native mod for Star Fox Adventures running through Foxhollow that lets you skip cutscenes.

## Controls

| Key | Action |
| --- | --- |
| F9 | Skip the current cutscene |
| F10 | Toggle automatic cutscene skipping (off by default) |

The keys only respond while the Foxhollow window has keyboard focus. On keyboards where the top row controls media or brightness by default, such as Mac keyboards, hold Fn while pressing F9 or F10.

## How it works

- Cutscenes are fast-forwarded through their normal sequence logic instead of jumping straight to the end.
- Game events, GameBits, item rewards, camera transitions and player-control transitions are preserved, so progression continues exactly as if the cutscene had played.
- Dialogue and input waits are intentionally preserved. Skipping stops when the game is waiting for you.
- Automatic skipping pauses at dialogue and input waits and continues after you advance them.
- Voice lines, sound effects and subtitles from the skipped cutscene are cleaned up while skipping.
- Skipping stops safely at map transitions and is protected against loops, stalls and runaway sequences.

## Installation

**Recommended:** install through the Foxhollow Launcher once the mod is published there.

**Manual:** place the extracted mod folder in the Foxhollow Launcher's `mods` folder, so it looks like this:

```
mods/
  cutscene-skip/
    mod.json
    lib/
      windows-amd64/
        mod.dll
      linux-amd64/
        mod.so
      linux-arm64/
        mod.so
      macos-x86_64/
        mod.so
      macos-arm64/
        mod.so
```

Foxhollow only loads the library in the folder that matches your system and ignores the others, so you only need the folder for your platform.

Restart the game after installing.

## Platform support

| Platform | Folder | Status |
| --- | --- | --- |
| Windows x64 | `windows-amd64` | Tested in game |
| Linux x86_64 | `linux-amd64` | Build validated, in-game testing pending |
| Linux ARM64 | `linux-arm64` | Build validated, in-game testing pending |
| macOS Apple Silicon | `macos-arm64` | Build validation pending |
| macOS Intel | `macos-x86_64` | Build validation pending |

Official Foxhollow builds are currently published for Windows x64, Linux x86_64 and macOS Apple Silicon. The Linux ARM64 and macOS Intel libraries are for Foxhollow builds you compile yourself. Windows on ARM is not supported.

On Linux and macOS, F9 and F10 are read from the keyboard state of Foxhollow's own SDL3 runtime, so the mod needs no extra libraries and behaves the same under X11 and Wayland.

## Known limitation

The original on-screen notifications are not displayed, because the current Foxhollow mod API does not expose the required supported rendering callback. Skipping works normally, and the log still reports `Cutscene Skipped`, `Cutscene Skip Enabled` and `Cutscene Skip Disabled`.

## Repository

https://github.com/saulob/Foxhollow-Cutscene-Skip

## License

[MIT](LICENSE)
