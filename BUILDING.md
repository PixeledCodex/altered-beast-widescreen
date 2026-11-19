# Build and run on Windows

Use this guide to build the preview from a fresh checkout of the repository.
The build downloads the pinned recompiler and controller library source,
generates C from your local ROM, compiles the Windows preview, and runs its tests.

## 1. Install the prerequisites

Install Git, Python 3.10 or newer, CMake 3.21 or newer, and **Visual Studio
2022 Build Tools**. In the Visual Studio installer, select **Desktop development
with C++** and include the x64 MSVC tools and a Windows SDK. Open a new
PowerShell window after installing them. The first build needs internet
access to download the pinned source archives.

Verify that PowerShell can find the tools:

```powershell
git --version
python --version
cmake --version
```

## 2. Check your ROM

Use your own raw, uncompressed Genesis / Mega Drive cartridge image of
**Altered Beast (USA, Europe) (Rev 2)**. It is **524,288 bytes** and has SHA-1
`38945360D824D2FB9535B4FD7F25B9AA9B32F019`. The filename and extension can vary.
Keep the file outside this repository.

Set the path to your copy and compare its hash:

```powershell
$rom = 'C:\path\to\your\Altered Beast (USA, Europe) (Rev 2).md'
Get-FileHash -LiteralPath $rom -Algorithm SHA1
```

## 3. Clone, build, and run

Use the same PowerShell window so `$rom` still points to your file.

```powershell
git clone https://github.com/PixeledCodex/altered-beast-widescreen.git
cd altered-beast-widescreen
python .\scripts\build_preview.py --rom $rom
```

Once the command reports three passing tests, use the launch command it prints:

```powershell
& .\build\standalone\preview\Release\widescreen_preview.exe $rom
```

Pass `--silent` after `$rom` to start without audio. Keep `SDL3.dll` beside
the executable. Building again reuses the verified dependency archives and
writes its outputs under `build/standalone/`.

## Troubleshooting

| What you see | What to check |
| --- | --- |
| ROM validation failed | Check the size and SHA-1 above. Unzip your own archive to select the image inside. |
| CMake cannot find Visual Studio | Check the C++ workload, x64 compiler, and Windows SDK in Visual Studio Installer. |
| A download failed | Check that your network can reach `codeload.github.com`, then rerun the build command. |
| A build or test failed | Open the log path printed for that step. Build logs are in `build/standalone/logs/`. |

To rerun the foreground edge, 400×224 stage / beast chooser, and controller
menu tests without recompiling:

```powershell
ctest --test-dir .\build\standalone\preview -C Release --output-on-failure
```
