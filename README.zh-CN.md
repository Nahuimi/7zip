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
  - 如果压缩包展开后包含多个顶层项目，则自动创建一个以压缩包文件名为基础的目录，再解压到该目录中。
  - 创建目录名时，会自动去掉常见分卷后缀，例如 `.7z.001`、`.part01.rar`。
- 资源管理器右键菜单：
  - 新增 `Smart Extract`
  - 菜单项位置在 `Extract files...` 下方

### 密码本与内置密码插件

这个分支增加了基于内置插件 DLL 的 Windows 密码本功能。

- 运行时相关文件：
  - `7zPasswordPlugins.dll`
  - `7zPasswordBook.db`
- 当前公开版本的行为：
  - 按压缩包 MD5 把密码保存到本地 SQLite 数据库
  - 可在 7zFM 的 `Password Book` 选项页里管理密码本
  - 支持通过 `7zPasswordBook.csv` 导入导出密码本条目
  - 解压时可预填已保存密码，并把确认可用的密码写回本地密码本
  - 在 7zFM 右键菜单中新增 `Query Password`
- 说明：
  - 当前公开构建只实现本地密码本查询和存储
  - 插件里的扩展查询导出目前仍保留为预留接口，公开版当前返回 `E_NOTIMPL`

### 每个文件/文件夹单独压缩

这个分支增加了一个 GUI 压缩选项，可为每个选中的输入项分别创建压缩包。

- 压缩对话框：
  - 新增 `Separate archive for each file/folder`
- 行为说明：
  - 选中多个文件或文件夹时，GUI 会为每个输入项分别生成一个压缩包
  - 每个生成的压缩包都会复用当前选择的归档格式和压缩参数
  - 如果输出名称冲突，会自动追加 `_2`、`_3` 等后缀避让

### 7zFM 与命令行的文件名编码选择和自动检测

这个分支还移植了来自 [Autori/7zip-codepage](https://github.com/Autori/7zip-codepage) 的 7zFM 文件名编码选择功能。

- 文件管理器：
  - 新增 `Tools -> Name Encoding`
  - 新增 `Auto Detect` 自动检测模式，用于归档条目名称编码识别
  - 支持 `UTF-8`、常见 OEM 代码页和常见 ANSI Windows 代码页
  - 切换编码后会刷新当前已打开的面板
- 自动检测：
  - 集成了 `compact_enc_det` 库，用于自动识别归档条目名称的编码
  - 会应用在走名称代码页检测流程的格式上，包括 ZIP 和 TAR
  - 如果检测结果不够可用，还会回退到内置启发式规则继续判断
- 命令行：
  - 支持 `-mcp=auto`，用于在支持 `cp` 属性的格式上自动识别归档条目名称编码
  - `cp` 属性也支持 `UTF-8`、`WIN`、`DOS` 和数字代码页
  - 示例：`7z x archive.zip -mcp=auto`
- 运行时行为：
  - 将所选代码页或 `auto` 模式保存到 `Z7_FORCE_CODEC` 环境变量中
  - 7zFM 打开压缩包时会应用该代码页
  - 如果没有在程序外部预先设置 `Z7_FORCE_CODEC`，则该设置只对当前 7zFM 进程生效

## 第三方许可证

本仓库为了归档条目名称自动检测，集成了 Google 的 `compact_enc_det` 库。

- 许可证：Apache License 2.0
- 源码许可证文本：[`CPP/Common/CompactEncDet/LICENSE`](CPP/Common/CompactEncDet/LICENSE)
- 第三方说明文件：[`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt)
- 引入的 `compact_enc_det` 源码树包含本仓库为集成和编译兼容性所做的本地修改

## GitHub Actions

Windows CI 定义位于 [`.github/workflows/build-windows.yml`](.github/workflows/build-windows.yml)。

- Runner：`windows-2022`
- 工具链：Visual Studio 2022 + `nmake`
- 目标：`x64`、`x86`
- 产物：
  - `7zip-windows-replacement-x64`：保持当前 Windows 发行目录结构的 `x64` 替换包，适合直接替换现有 7-Zip 安装或现有打包结构中的构建产物；同时会包含 `Licenses/` 目录，内含 7-Zip 许可证、`compact_enc_det` 的 Apache-2.0 许可证文本以及第三方说明文件
  - `7zip-windows-replacement-x86`：对应的 `x86` 替换包 zip
  - `7zip-windows-all-products`：把所有生成的 `.exe`、`.dll`、`.sfx`、语言文件和同样的 `Licenses/` 文档一起打进一个 zip 包，便于检查、下载或二次分发

Windows 安装包发布流程定义位于 [`.github/workflows/release-windows-installer.yml`](.github/workflows/release-windows-installer.yml)。

- 触发方式：手动 `workflow_dispatch`
- 输入参数：`version_tag`
  - 必须以 `v<主版本>.<次版本>` 开头，例如 `v26.00` 或 `v26.00-0.0.1`
- 行为：
  - 构建 `x64` 和 `x86` Windows 二进制
  - 根据标签前缀里的 `v<主版本>.<次版本>` 下载对应上游 7-Zip 安装包骨架
  - 用仓库中的构建产物、`Lang/` 里的语言文件，以及 `DOC/` 中选定的发行文档替换安装包内容
  - 创建或更新 GitHub Release，并上传生成好的安装器 `.exe`
- 安装包内容说明：
  - 包含 `Uninstall.exe`
  - 包含 `7zPasswordPlugins.dll`，卸载时也会删除它
  - 不安装 `Install.exe`
  - 不安装 `7zS.sfx`
