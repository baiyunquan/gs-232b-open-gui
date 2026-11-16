# 在 Linux 上用 MSVC 编译 Windows 版本

`scripts/build-windows.sh` 面向 Linux x86_64 主机，通过 [msvc-wine](https://github.com/mstorsjo/msvc-wine) 在 Wine 中运行微软的 `cl.exe`、链接器和 Windows SDK 工具。CMake、Ninja 和 Qt 主机构建工具在 Linux 上运行。目标系统为 Windows 10（1809 或更新版本）/11 x64。

Windows 程序、静态依赖和 Qt DLL 统一使用 MSVC ABI 与动态 MSVC/UCRT 运行库（Release 使用 `/MD`）。不再使用 MinGW，也不再把 UCRT 编译器的运行库与旧 MSVCRT Qt SDK 混合打包。

## 安装依赖

Arch Linux：

```sh
sudo pacman -S --needed cmake ninja python python-pip wine msitools curl tar binutils
```

需要 CMake 3.25 或更新版本。构建工具链使用嵌入式调试信息，CMake 的编译器探测不依赖 `winbind`。安装系统依赖后，SDK 安装和编译无需管理员权限。

首次运行下载：

- 固定版本的 `msvc-wine`（提交 `5dd115047b7ddcbb53cbc8b5b14163a20f4334fe`，校验 SHA-256），以及微软 MSVC 14.44（VS 2022 v143）、Windows SDK 10.0.26100。微软组件由下载器根据官方安装清单校验。
- `aqtinstall 3.3.0` 和 Qt 6.8.3：Linux QtBase、QtTools、ICU，以及 Windows **msvc2022_64** 的 QtBase、QtTools、QtTranslations、QtSerialPort、QtWebSockets。
- 固定版本并校验 SHA-256 的 anytermqt 和 utf8proc；这两个依赖使用同一 MSVC 静态编译，无需额外 DLL。

SDK 下载器使用独立 Python 虚拟环境，默认使用阿里云 Qt 镜像并保留下载校验。首次安装微软工具链时，下载器会要求接受[微软许可](https://go.microsoft.com/fwlink/?LinkId=2086102)。阅读并接受后，可以用 `MSVC_ACCEPT_LICENSE=1` 进行非交互安装；脚本不会默认替用户接受许可。MSVC/SDK 安装目录是本机的构建缓存，不应随应用分发。

## 构建与输出

从项目根目录运行：

```sh
./scripts/build-windows.sh

# 已阅读并接受微软许可时，非交互首次安装并构建。
MSVC_ACCEPT_LICENSE=1 ./scripts/build-windows.sh
```

脚本可从其他工作目录通过绝对路径调用。默认并行编译数为 2，可通过 `JOBS=4` 调整。

成功后生成：

- `build/windows-msvc-x86_64/package/gs_232b_open_gui.exe`：Windows 程序。
- `build/windows-msvc-x86_64/package/`：完整程序目录，包含 Qt DLL、配套 MSVC 可再分发 DLL、平台插件、中文 Qt 翻译、许可证和 `qt.conf`。
- `build/windows-msvc-x86_64/gs-232b-open-gui-windows-x86_64.zip`：可分发 ZIP 包。

在 Windows 上解压 ZIP 后运行 `gs_232b_open_gui.exe`，保留完整目录。MSVC CRT 随程序部署，UCRT 由支持的 Windows 系统提供。应用自身的中文翻译嵌入 EXE。Windows 虚拟串口仍需预先安装配对驱动并创建 COM 对。

脚本只构建程序，关闭测试目标。打包时通过独立 Wine 前缀运行所选 Qt SDK 的 `windeployqt.exe`，然后递归检查所有 EXE/DLL 的架构和运行库导入；发现旧 `msvcrt.dll`、MinGW 运行库或 Debug CRT 会阻止生成新包。MSVC CRT 仅取自所选工具链的 x64 可再分发目录，不从 Wine 或系统中补 DLL。Wine 下的运行验证不能代替真实 Windows 上的设备连接验证。

## 缓存与自定义路径

SDK、下载器和独立 Wine 前缀默认缓存在 `${XDG_CACHE_HOME:-$HOME/.cache}/gs232b-windows`。再次运行复用缓存并增量编译。

```sh
# 改用 Qt 官方下载源。
QT_MIRROR=https://download.qt.io ./scripts/build-windows.sh

# 自定义缓存和构建目录。
WINDOWS_BUILD_CACHE=/path/to/cache BUILD_DIR=/path/to/build ./scripts/build-windows.sh

# 复用已通过 msvc-wine 安装的工具链和同版本 Qt SDK。
MSVC_ROOT=/path/to/msvc \
QT_HOST_PATH=/path/to/Qt/6.8.3/gcc_64 \
QT_WINDOWS_PREFIX=/path/to/Qt/6.8.3/msvc2022_64 \
./scripts/build-windows.sh
```

`MSVC_ROOT` 必须包含 `msvc-wine` 安装生成的 `bin/x64/{cl,link,lib,rc,mt}` 包装器。自备 Windows Qt 必须是 Qt 6.8.3 **msvc2022_64**，且含上述模块。`MSVC_REDIST_DIR` 可显式指定配套的 x64 `Microsoft.VC143.CRT` 目录。

旧的 `build/windows-x86_64` 目录和其中 ZIP 含不兼容的 MinGW/MSVCRT 组合，不能继续分发。新默认目录 `build/windows-msvc-x86_64` 避免复用旧缓存；显式指定含其他工具链缓存的 `BUILD_DIR` 时脚本会拒绝继续。更换工具链或 SDK 时也应使用新目录。

运行 `./scripts/build-windows.sh --help` 可查看所有环境变量。可独立复查任意目录：

```sh
python3 scripts/verify-windows-runtime.py build/windows-msvc-x86_64/package

# 运行打包运行库约束的回归检查，无需安装 Windows SDK。
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_windows_runtime.py
```
