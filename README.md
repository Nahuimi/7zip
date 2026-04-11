# 7-Zip on GitHub

7-Zip website: [7-zip.org](https://7-zip.org)

中文文档： [README.zh-CN.md](README.zh-CN.md)

## Overview

This repository is a fork of 7-Zip with several Windows-focused enhancements on top of the upstream codebase.

The source tree mainly includes:

- `CPP/7zip/`: main C++ implementation and Windows application code
- `C/`: low-level codec and utility code
- `Asm/`: hand-written assembly optimizations
- `DOC/`: upstream documentation and format references
- `Lang/`: localization resources

## Custom Changes

### Smart Extract

This fork adds an intelligent extract mode for Windows builds.

- Command line: `7z sx archive.7z`
- Behavior:
  - If the archive expands to a single top-level file or folder, extract directly to the target path.
  - If the archive expands to multiple top-level items, create a folder named after the archive and extract into it.
  - Common multi-volume suffixes such as `.7z.001` and `.part01.rar` are stripped from the created folder name.
- Explorer context menu:
  - Adds `Smart Extract`
  - The entry is placed below `Extract files...`

### Name Encoding Selector and Auto Detection in 7zFM

This fork also ports the 7zFM name-encoding selector from [Autori/7zip-codepage](https://github.com/Autori/7zip-codepage).

- File Manager:
  - Adds `Tools -> Name Encoding`
  - Adds an `Auto Detect` mode for archive entry names
  - Supports `UTF-8`, common OEM code pages, and common ANSI Windows code pages
  - Refreshes open panels after switching the selected code page
- Auto detection:
  - Uses the integrated `compact_enc_det` library to detect archive entry name encodings automatically
  - Applies to formats that route through the name code-page detection path, including ZIP and TAR
  - Falls back to internal heuristics when the detector cannot produce a usable result
- Runtime behavior:
  - Stores the selected code page or the `auto` mode in the `Z7_FORCE_CODEC` environment variable
  - Applies the selected code page when opening archives in 7zFM
  - Keeps the setting only for the current 7zFM process unless `Z7_FORCE_CODEC` is set outside the app

## Build

### Windows with `nmake`

Open a Visual Studio Developer Prompt first, or run the matching `vcvars*.bat` script before building.

Build the main Windows tree:

```bat
cd CPP\7zip
nmake
```

Build only `7za.exe`:

```bat
cd CPP\7zip\Bundles\Alone
nmake
```

### GCC / MinGW / Unix-like builds

Build `7zz` with the bundled GCC makefile:

```sh
cd CPP/7zip/Bundles/Alone2
make -j -f makefile.gcc
```

Build the optimized x64 GCC variant:

```sh
make -j -f ../../cmpl_gcc_x64.mak
```

Windows outputs usually land under `o/` or `<PLATFORM>/`. GCC and Clang builds typically use `b/g*` or `b/c*`.

## Verification

This repository does not include a separate automated test directory. After building, a simple smoke test can be done with:

```bat
.\7za.exe a test.7z .\DOC
.\7za.exe t test.7z
```

For parser or extractor changes, verify open, list, test, and extract paths with representative sample archives.

## GitHub Actions

Windows CI is defined in [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml).

- Runner: `windows-2022`
- Toolchain: Visual Studio 2022 + `nmake`
- Targets: `x64`, `x86`
- Artifacts:
  - `7zip-windows-replacement`: replacement package for the current Windows build outputs
  - `7zip-windows-all-products`: zip archive with all collected `.exe`, `.dll`, `.sfx` products and language files
