"""Build and test the Windows preview using a verified local ROM."""
import argparse
import csv
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
from urllib.request import urlopen
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
SUPPORT = ROOT / "build_support"
ROM_SHA256 = "cbe8baeab2efe531a674f53bfddd0c7944cfe2dcda6446ef300f98254f8e80bc"
ARCHIVES = (
    ("genesisrecomp", "mstan/segagenesisrecomp", "9cedc86d96c4886919083e4ab2321aa22ffa4078",
     "3c6165f3363eac940afcd30fa658f5e9ab45815637a613a16611f16255de5566"),
    ("m68k-recomp-core", "mstan/m68k-recomp-core", "03ba2787da829369bbc2d958621b10330885c8f8",
     "8a5286c688ed2ba3064f77704b8efce5f61dd85300711ff4ba348c14af3ce0be"),
    ("SDL3", "libsdl-org/SDL", "f00dc919ca35b9756316e86c4777392de442f9b2",
     "c6e16e813190d4149098026ef390c6f63257a2439a28c751adc957171aba6578"),
)
PATCHES = (
    ("frontend-ram", "genesisrecomp/recompiler/src/main_step.c"),
    ("frontend-wide", "genesisrecomp/recompiler/src/main_step.c"),
    ("interrupt", "genesisrecomp/external/m68k-recomp-core/runtime/m68k_step.c"),
    ("vdp", "genesisrecomp/runner/video/genesis_vdp.c"),
)
WIDE_SITES = {0x42D2, 0x42D8, 0x42FE, 0x4304, 0x4340, 0x4346}
ROM_ENTRY_COUNT = 10493
# The boot loader copies this 440-byte span into RAM. It has two entries.
RAM_ARGS = ("0xFFF200", "0xFFF200", "0xFFF214", "--input-span", "0x450E", "440")
GENERATOR = ("-G", "Visual Studio 17 2022", "-A", "x64")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_if_changed(path, data):
    if path.is_file() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def run(logs, label, *command, cwd=ROOT, env=None):
    print(label, flush=True)
    log = (logs / label).with_suffix(".log")
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        tail = log.read_text(encoding="utf-8", errors="replace").splitlines()[-24:]
        raise RuntimeError(f"{label} failed ({result.returncode}). See {log}\n" + "\n".join(tail))
    return log


def fetch_archive(cache, name, repo, revision, expected):
    archive = cache / f"{name}.zip"
    if archive.is_file() and digest(archive) == expected:
        return archive
    url = f"https://codeload.github.com/{repo}/zip/{revision}"
    print(f"Downloading {name} at {revision[:7]}", flush=True)
    with TemporaryDirectory(dir=cache) as scratch:
        download = Path(scratch) / "source.zip"
        with urlopen(url, timeout=45) as response, download.open("wb") as stream:
            size = 0
            while chunk := response.read(256 * 1024):
                size += len(chunk)
                if size > 32 * 1024 * 1024:
                    raise ValueError(f"Unexpectedly large archive from {url}")
                stream.write(chunk)
        if digest(download) != expected:
            raise ValueError(f"{name} archive SHA-256 differs from the pinned download")
        download.replace(archive)
    return archive


def extract(archive, destination, name, archive_root):
    with ZipFile(archive) as source:
        for member in source.infolist():
            path = PurePosixPath(member.filename)
            if path.parts[0] != archive_root or ".." in path.parts or "\\" in member.filename:
                raise ValueError(f"Unexpected source path in {archive.name}")
            if member.is_dir():
                continue
            relative = PurePosixPath(*path.parts[1:])
            if name == "genesisrecomp" and relative.as_posix() not in {
                "LICENSE.md", "LICENSE-recompiler", "THIRD-PARTY-LICENSES.md", "runner/cosim.h",
            } and not relative.as_posix().startswith((
                "recompiler/src/", "runner/video/", "runner/audio/", "runner/include/",
                "runner/external/superzazu/", "runner/external/ymfm/",
            )):
                continue
            write_if_changed(destination.joinpath(*relative.parts), source.read(member))


def prepare_dependencies(build, inputs, logs):
    cache = build / "downloads"
    cache.mkdir(exist_ok=True)
    for name, repo, revision, expected in ARCHIVES:
        archive = fetch_archive(cache, name, repo, revision, expected)
        destination = "genesisrecomp/external/m68k-recomp-core" if name == "m68k-recomp-core" else name
        extract(archive, inputs / destination, name, f"{repo.split('/')[1]}-{revision}")

    # Apply deltas to private copies of the pinned files.
    overlay = inputs / "overlays"
    patch_env = {**os.environ, "GIT_CEILING_DIRECTORIES": str(overlay)}
    with TemporaryDirectory(dir=overlay) as scratch:
        for name, source in PATCHES:
            target = Path(scratch) / Path(source).name
            target.write_bytes((inputs / source).read_bytes().replace(b"\r\n", b"\n"))
            run(logs, f"apply-{name}", "git", "apply", "--no-index",
                str(SUPPORT / "patches" / f"{name}.patch"), cwd=target.parent, env=patch_env)
            write_if_changed(overlay / name / target.name, target.read_bytes())
            target.unlink()


def generate_code(build, inputs, logs, rom, jobs):
    compiler = build / "compiler"
    run(logs, "configure-code-generators", "cmake", "-S", str(SUPPORT / "compiler"),
        "-B", str(compiler), *GENERATOR, f"-DAB_INPUTS={inputs}")
    run(logs, "compile-code-generators", "cmake", "--build", str(compiler), "--config",
        "Release", "--parallel", str(jobs))

    entries = str(SUPPORT / "rom-entries.txt")
    programs = compiler / "Release"
    modules = (("rom", "ab_rom", ROM_ENTRY_COUNT), ("ram", "ab_ram", 34),
               ("wide", "ab_right", ROM_ENTRY_COUNT))
    with TemporaryDirectory(dir=build) as scratch:
        work = Path(scratch)
        for folder, _, _ in modules:
            (work / folder).mkdir()
        run(logs, "generate-ROM-code", str(programs / "generate_rom.exe"), str(rom),
            str(work / "rom"), "ab_rom", "0", "--entries-file", entries)
        run(logs, "generate-copied-RAM-code", str(programs / "generate_ram.exe"), str(rom),
            str(work / "ram"), "ab_ram", *RAM_ARGS)

        # The wide module preserves original guards and timings. Its private
        # generator changes only the semantics at the six verified sprite sites.
        pattern = re.compile(r" \{0x([0-9A-F]+)u,\{([^}]+)\},(\d+),(\d+),")
        base = (work / "rom/ab_rom_module.c").read_text(encoding="ascii")
        catalog = {int(pc, 16): (words, count, cycles)
                   for pc, words, count, cycles in pattern.findall(base)}
        if len(catalog) != ROM_ENTRY_COUNT or not WIDE_SITES <= catalog.keys():
            raise ValueError("Generated ROM module differs from the reviewed operation catalog")
        covered = work / "covered.txt"
        covered.write_text("".join(
            f"{pc:#x} {count} {cycles} {words.replace(',', ' ')}\n"
            for pc, (words, count, cycles) in catalog.items() if pc not in WIDE_SITES
        ), encoding="ascii")
        run(logs, "generate-widescreen-code", str(programs / "generate_wide.exe"), str(rom),
            str(work / "wide"), "ab_right", "0", "--entries-file", entries,
            "--covered-entries-file", str(covered))

        for folder, stem, expected in modules:
            with (work / folder / f"{stem}_coverage.csv").open(encoding="ascii", newline="") as stream:
                count = sum(1 for _ in csv.DictReader(stream))
            if count != expected:
                raise ValueError(f"{stem} generated {count} operations, expected {expected}")
            for path in (work / folder).iterdir():
                write_if_changed(inputs / "generated" / folder / path.name, path.read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True, help="your Altered Beast UE REV02 ROM image")
    parser.add_argument("--jobs", type=int, default=2, help="parallel compile jobs (default 2)")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if sys.platform != "win32":
        parser.error("Use 64-bit Windows with Visual Studio 2022 Build Tools")
    try:
        rom = args.rom.resolve(strict=True)
        if not rom.is_file() or rom.stat().st_size != 524288 or digest(rom) != ROM_SHA256:
            raise ValueError("ROM must match the 512 KiB Genesis/Mega Drive UE REV02 dump in README.md")
        print("Verified the local UE REV02 ROM", flush=True)
        for tool in ("git", "cmake", "ctest"):
            if not shutil.which(tool):
                raise ValueError(f"{tool} must be on PATH. See the README prerequisites.")
        build = ROOT / "build" / "standalone"
        inputs, logs = build / "inputs", build / "logs"
        (inputs / "overlays").mkdir(parents=True, exist_ok=True)
        logs.mkdir(exist_ok=True)
        prepare_dependencies(build, inputs, logs)
        generate_code(build, inputs, logs, rom, args.jobs)
        preview = build / "preview"
        run(logs, "configure-preview", "cmake", "-S", str(ROOT), "-B", str(preview),
            *GENERATOR, f"-DAB_INPUTS={inputs}", f"-DAB_TEST_ROM={rom}", "-DBUILD_TESTING=ON")
        run(logs, "compile-preview", "cmake", "--build", str(preview), "--config", "Release",
            "--parallel", str(args.jobs))
        report = run(logs, "test-preview", "ctest", "--test-dir", str(preview), "-C",
                     "Release", "--output-on-failure", "--no-tests=error")
        print(report.read_text(encoding="utf-8", errors="replace").rstrip())
        print(f'\nRun the preview in PowerShell:\n  & "{preview / "Release/widescreen_preview.exe"}" "{rom}"')
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"Build stopped: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
