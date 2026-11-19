# Altered Beast widescreen preview

An experimental native Windows preview of the Genesis / Mega Drive version.
It renders a fixed **400×224** view with 40 extra pixels on each side of the
original 320×224 image.

## Get started

| What you need | Supported |
| --- | --- |
| Platform | Windows x64 |
| Game image | Your own raw **Altered Beast (USA, Europe) (Rev 2)** ROM |
| Distribution | Source build. There is currently no prebuilt download. |

[Build and run the preview](BUILDING.md) for the exact ROM checksum,
prerequisites, build command, and executable path.

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Move | Arrow keys | D-pad or left stick |
| A / B / C | Z / X / C | Left / bottom / right face buttons |
| Start | Enter | Start or Menu |
| Pause menu | Esc | Select or Back |

**Beast select (restarts)** in the pause menu restarts the run into the
original per-round chooser. Use Up or Down to pick a round and Left or Right
to choose its beast. Press Start to return to the title screen, then start
a new game with your choices.

## What widescreen covers

The preview extends foreground edge tiles near the start of Stages 1, 2, 3,
and 5. Stage 4 and scripted demos keep their original edge behavior. Wider
scenery streaming, enemy activation outside the original view, and full-game
parity remain open.

## How it works

Git tracks the project's source and GitHub hosts a copy. When you build,
GenesisRecomp reads your verified ROM and translates 68000 instructions to C
on your computer. The disassembly helped identify instruction addresses and
verify the widescreen changes. The repository contains the resulting address
metadata, preview source, and tests. ROMs, disassembly files, generated game
code, and binaries stay outside the repository.

[See the credits and pinned upstream revisions.](ACKNOWLEDGMENTS.md)

If you report a problem, include your commit (`git rev-parse --short HEAD`),
Windows version, and the failing build step or in-game stage. Include steps
to reproduce and any relevant text from the log. Keep your ROM local.
