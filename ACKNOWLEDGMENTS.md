# Acknowledgments

This preview builds on the work of the original game developers, ROM
researchers, and emulator and recompiler authors.

## Original game and source research

- **SEGA** developed and published Altered Beast. The preview uses the player's
  own Genesis/Mega Drive UE REV02 ROM.
- **Hans Bonini and contributors** reconstructed the 68000 assembly used to
  check the reference ROM and verify addresses and instruction sites for the
  widescreen changes. [smd_alteredbeast at `f924507`](https://github.com/hansbonini/smd_alteredbeast/tree/f9245076a83655fdce75edf16b244527b6879422)

## Recompiler and runtime

- **Matthew Stan and GenesisRecomp contributors** provided the 68000-to-C
  recompiler and the VDP and SN76489 PSG code used by the local runtime.
  [GenesisRecomp at `9cedc86`](https://github.com/mstan/segagenesisrecomp/tree/9cedc86d96c4886919083e4ab2321aa22ffa4078) · [PolyForm Noncommercial 1.0.0](https://github.com/mstan/segagenesisrecomp/blob/9cedc86d96c4886919083e4ab2321aa22ffa4078/LICENSE.md)
- **Matthew Stan and m68k-recomp-core contributors** provided the shared
  compiler support and 68000 execution runtime used with the generated code.
  [m68k-recomp-core at `03ba278`](https://github.com/mstan/m68k-recomp-core/tree/03ba2787da829369bbc2d958621b10330885c8f8) · [PolyForm Noncommercial 1.0.0](https://github.com/mstan/m68k-recomp-core/blob/03ba2787da829369bbc2d958621b10330885c8f8/LICENSE)
- **Nicolas Allemand** wrote the Z80 core that runs the game's sound driver.
  The local runtime uses GenesisRecomp's vendored version with its patches.
  [superzazu/z80 at `d64fe10`](https://github.com/superzazu/z80/tree/d64fe10a2274e5e40019b1086bf7d8990cbc5f23) · [MIT license](https://github.com/superzazu/z80/blob/d64fe10a2274e5e40019b1086bf7d8990cbc5f23/LICENSE)
- **Aaron Giles** wrote ymfm. The runtime uses the GenesisRecomp vendored
  YM2612 core for FM synthesis.
  [ymfm at `17decfa`](https://github.com/aaronsgiles/ymfm/tree/17decfae857b92ab55fbb30ade2287ace095a381) · [BSD 3-Clause license](https://github.com/aaronsgiles/ymfm/blob/17decfae857b92ab55fbb30ade2287ace095a381/LICENSE)
- **Sam Lantinga and the SDL contributors** provided the SDL 3.4.20 gamepad
  and joystick support used by the Windows preview.
  [SDL 3.4.20](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20) · [zlib license](https://github.com/libsdl-org/SDL/blob/release-3.4.20/LICENSE.txt)


## Independent validation

- **Genesis Plus GX and libretro contributors** provided the reference core
  and API used by the original-view comparison harness. The runtime also
  uses its documented interrupt timing as a reference. The comparison core
  ran separately from the widescreen executable.
  [Genesis Plus GX at `5355eae`](https://github.com/libretro/Genesis-Plus-GX/tree/5355eae2e1c70893a14ec0fda68de4e49cd5a0d5) · [libretro API](https://github.com/libretro/RetroArch/blob/master/libretro-common/include/libretro.h)

The original game and upstream components retain their respective rights and
licenses. See [GenesisRecomp's third-party notices](https://github.com/mstan/segagenesisrecomp/blob/9cedc86d96c4886919083e4ab2321aa22ffa4078/THIRD-PARTY-LICENSES.md)
for the components it vendors. The project source license remains open.

## Build patches

The four patches under `build_support/patches/` apply to local copies of
the pinned upstream sources. They carry forward copied-RAM generation, the
six widened sprite comparisons, and the interrupt and VDP fixes used by
the preview. Upstream fragments in those patches are by Matthew Stan and the
GenesisRecomp and m68k-recomp-core contributors and retain their upstream
[PolyForm Noncommercial 1.0.0 terms](https://polyformproject.org/licenses/noncommercial/1.0.0/).
