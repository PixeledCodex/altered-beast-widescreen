# Altered Beast widescreen preview

Experimental Windows C core and standalone SDL3 host for the Genesis/Mega Drive
UE REV02 release. The preview renders a centered 400×224 view, adding 40 pixels
on each side of the original 320×224 image. It keeps the original scanline
render for hardware-visible state and renders the wider image from a separate
copy. The foreground edge extension applies at the start of Stages 1, 2, 3
and 5. Stage 4 and scripted demos are outside that extension. Broader scenery
streaming, actor activation and full-game parity remain open.

## Build on Windows

Use Visual Studio 2022 Build Tools with the x64 C++ workload and CMake 3.20
or newer. This repository builds against an existing qualified
AlteredBeast-SourcePort workspace. CMake checks the generated widescreen
module and eight runtime library hashes before building. A fresh checkout
still needs that local workspace and its build outputs.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DAB_SOURCEPORT_ROOT="C:/path/to/AlteredBeast-SourcePort"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\widescreen_preview.exe 'C:\path\to\your-owned-UE-REV02.md'
```

Omit `AB_SOURCEPORT_ROOT` when both checkouts are siblings. Use the UE REV02 ROM
with SHA-1 38945360D824D2FB9535B4FD7F25B9AA9B32F019. Pass --silent to disable
audio output. Keyboard controls are arrows, Z/X/C, and Enter. Esc opens the
paused menu. SDL3 supports controllers.

Enable the original beast chooser and controller menu tests by passing
-DAB_TEST_ROM="C:/path/to/your-owned-UE-REV02.md" when configuring, then build
and run CTest again. Its captures stay in the ignored build directory.

## Source and dependencies

The source snapshot follows the qualified Task 32 widescreen preview
(run-debcd3f38f0a42e89b93d79d2da3683f) and uses the qualified runtime
e21a6f4dad5d4689aadba631bd098267. The local baseline's launch check and
validation manifest document the earlier runtime and frame verification.

Git tracks the preview C code, its build configuration, and two focused tests.
The local workspace supplies the generated 68000 code, runtime libraries and
headers, and SDL3 3.4.20. Keep ROMs, original game assets, generated game code,
diagnostic captures, and binaries outside Git. See
[Acknowledgments](ACKNOWLEDGMENTS.md) for project credits, pinned upstream
revisions, and dependency licenses.
