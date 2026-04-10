# GitHub 上的 7-Zip

7-Zip 官网：[7-zip.org](https://7-zip.org)

英文文档：[README.md](README.md)

## 项目简介

这个仓库是 7-Zip 的一个分支版本，在上游代码基础上增加了一些偏向 Windows 使用场景的增强功能。

仓库中的主要目录包括：

- `CPP/7zip/`：主要 C++ 实现和 Windows 应用代码
- `C/`：底层编解码与工具代码
- `Asm/`：手写汇编优化代码
- `DOC/`：上游文档与格式说明
- `Lang/`：本地化资源

## 自定义改动

### 智能解压（Smart Extract）

这个分支为 Windows 构建增加了智能解压模式。

- 命令行：`7z sx archive.7z`
- 行为说明：
  - 如果压缩包展开后只有一个顶层文件或文件夹，则直接解压到目标路径。
  - 如果压缩包展开后包含多个顶层项目，则自动创建一个以压缩包命名的文件夹，再解压到该目录中。
  - 创建目录名时，会自动去掉常见分卷后缀，例如 `.7z.001`、`.part01.rar`。
- 资源管理器右键菜单：
  - 新增 `Smart Extract`
  - 菜单项位置在 `Extract files...` 下方

### 7zFM 文件名编码选择器

这个分支还移植了来自 [Autori/7zip-codepage](https://github.com/Autori/7zip-codepage) 的 7zFM 文件名编码选择功能。

- 文件管理器：
  - 新增 `Tools -> Name Encoding`
  - 支持 `UTF-8`、常见 OEM 代码页和常见 ANSI Windows 代码页
  - 切换编码后会刷新当前已打开的面板
- 运行时行为：
  - 将所选代码页保存到 `Z7_FORCE_CODEC` 环境变量中
  - 7zFM 打开压缩包时会应用该代码页
  - 如果没有在程序外部预先设置 `Z7_FORCE_CODEC`，则该设置只对当前 7zFM 进程生效

## 基本验证

本仓库没有独立的自动化测试目录。构建完成后，可以使用下面的命令做一个简单的冒烟测试：

```bat
.\7za.exe a test.7z .\DOC
.\7za.exe t test.7z
```

如果改动涉及压缩包解析或解压逻辑，建议额外验证打开、列表、测试、解压等路径，并准备有代表性的样例压缩包进行检查。

## GitHub Actions

Windows CI 定义位于 [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml)。

- Runner：`windows-2022`
- 工具链：Visual Studio 2022 + `nmake`
- 目标：`x64`、`x86`
- 产物：
  - `7zip-windows-replacement`：当前 Windows 构建输出的替换包
  - `7zip-windows-all-products`：包含所有收集到的 `.exe`、`.dll`、`.sfx` 产物及语言文件的 zip 包