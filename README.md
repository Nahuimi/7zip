# 7-Zip on GitHub

7-Zip website: [7-zip.org](https://7-zip.org)

## Custom Changes

This fork adds an intelligent extract mode for Windows builds.

- Command line: `7z sx archive.7z`
- Behavior:
  - If the archive expands to a single top-level file or folder, extract directly to the target path.
  - If the archive expands to multiple top-level items, create a folder named after the archive and extract into it.
  - Common multi-volume suffixes such as `.7z.001` and `.part01.rar` are stripped from the created folder name.
- Explorer context menu:
  - Adds `Smart Extract`
  - The entry is placed below `Extract files...`

## GitHub Actions

Windows CI is defined in [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml).

- Runner: `windows-2022`
- Toolchain: Visual Studio 2022 + `nmake`
- Targets: `x64`, `x86`
- Artifacts: built `.exe`, `.dll`, `.sfx` files collected from the generated platform output directories
