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

## Third-Party Licenses

This repository vendors Google's `compact_enc_det` library for archive entry name auto-detection.

- License: Apache License 2.0
- Source license text: [`CPP/Common/CompactEncDet/LICENSE`](CPP/Common/CompactEncDet/LICENSE)
- Third-party notice summary: [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt)
- The imported `compact_enc_det` source tree contains local modifications for integration and compiler compatibility

## GitHub Actions

Windows CI is defined in [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml).

- Runner: `windows-2022`
- Toolchain: Visual Studio 2022 + `nmake`
- Targets: `x64`, `x86`
- Artifacts:
  - `7zip-windows-replacement`: a replacement package that keeps the layout of the current Windows distribution, suitable for directly swapping built binaries into an existing 7-Zip installation or package structure; it also includes a `Licenses/` directory with the 7-Zip license, the bundled `compact_enc_det` Apache-2.0 license text, and third-party notices
  - `7zip-windows-all-products`: a zip archive that collects all produced `.exe`, `.dll`, `.sfx`, language files, and the same `Licenses/` documentation in one place for inspection, download, or redistribution
