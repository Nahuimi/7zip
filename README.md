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

This fork also ports the 7zFM name-encoding selector from [Autori/7zip-codepage](https://github.com/Autori/7zip-codepage).

- File Manager:
  - Adds `Tools -> Name Encoding`
  - Supports `UTF-8`, common OEM code pages, and common ANSI Windows code pages
  - Refreshes open panels after switching the selected code page
- Runtime behavior:
  - Stores the selected code page in the `Z7_FORCE_CODEC` environment variable
  - Applies the selected code page when opening archives in 7zFM
  - Keeps the setting only for the current 7zFM process unless `Z7_FORCE_CODEC` is set outside the app

## GitHub Actions

Windows CI is defined in [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml).

- Runner: `windows-2022`
- Toolchain: Visual Studio 2022 + `nmake`
- Targets: `x64`, `x86`
- Artifacts:
  - `7zip-windows-replacement`: replacement package for the current Windows build outputs
  - `7zip-windows-all-products`: zip archive with all collected `.exe`, `.dll`, `.sfx` products and language files
