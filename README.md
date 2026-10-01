# Foxhollow Cutscene Skip

A native mod for Star Fox Adventures running through Foxhollow that lets you skip cutscenes.

## Controls

| Key | Action |
| --- | --- |
| F9 | Skip the current cutscene |
| F10 | Toggle automatic cutscene skipping (off by default) |

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
```

Restart the game after installing.

## Platform support

- Windows x64

Other Foxhollow platforms are not supported by this mod yet

## Known limitation

The original on-screen notifications are not displayed, because the current Foxhollow mod API does not expose the required supported rendering callback. Skipping works normally, and the log still reports `Cutscene Skipped`, `Cutscene Skip Enabled` and `Cutscene Skip Disabled`.

## Repository

https://github.com/saulob/Foxhollow-Cutscene-Skip

## License

[MIT](LICENSE)
