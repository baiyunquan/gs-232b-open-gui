# 在 Linux 上编译 Windows 版本

`scripts/build-windows.sh` 面向 Linux x86_64 主机，使用 MinGW-w64 交叉编译 Windows x64 Release 程序。当前 Arch Linux 可直接使用；目标系统为 Windows 10（1809 或更新版本）/11 x64。

## 安装依赖

Arch Linux：

```sh
sudo pacman -S --needed cmake ninja python python-pip mingw-w64-gcc wine
```

SDK 下载器使用单独的 Python 虚拟环境，不向系统 Python 安装包。首次运行自动下载固定版本的 `aqtinstall 3.3.0` 和 Qt 6.8.3：Linux 的 QtBase、QtTools 及配套 ICU，以及 Windows MinGW 的 QtBase、QtTools、QtTranslations、QtSerialPort、QtWebSockets。默认使用阿里云 Qt 镜像并保留下载校验。Linux Qt 提供 `moc`、`rcc` 和 `lrelease` 等构建工具；Windows Qt 提供目标头文件、库和部署工具。

## 构建与输出

从项目根目录运行：

```sh
./scripts/build-windows.sh
```

脚本也可从其他工作目录通过绝对路径调用。默认并行编译数为 2，可通过 `JOBS=4 ./scripts/build-windows.sh` 调整。

成功后生成：

- `build/windows-x86_64/package/gs_232b_open_gui.exe`：Windows 程序。
- `build/windows-x86_64/package/`：包含 Qt DLL、MinGW 运行库、Windows 平台插件、中文 Qt 翻译及 `qt.conf` 的程序目录。
- `build/windows-x86_64/gs-232b-open-gui-windows-x86_64.zip`：包含完整程序目录的 ZIP 包。

在 Windows 上解压 ZIP 后运行 `gs_232b_open_gui.exe`。分发时保留完整目录。应用自身的中文翻译已嵌入 EXE；Windows 虚拟串口仍需预先安装配对驱动并创建 COM 对，详见 [虚拟串口说明](virtual-serial.md)。

该脚本关闭测试目标，只构建程序。打包通过独立 Wine 前缀运行 Qt 官方 `windeployqt.exe`；这一步部署 Qt 依赖，不能代替真实 Windows 上的设备连接验证。

## 缓存与自定义路径

SDK、Python 虚拟环境和独立 Wine 前缀默认缓存在 `${XDG_CACHE_HOME:-$HOME/.cache}/gs232b-windows`。再次运行复用缓存并增量编译。构建产物位于已忽略的 `build/` 内，不影响原有 Linux 构建目录。

```sh
# 改用 Qt 官方下载源。
QT_MIRROR=https://download.qt.io ./scripts/build-windows.sh

# 自定义缓存和构建目录。
WINDOWS_BUILD_CACHE=/path/to/cache BUILD_DIR=/path/to/build ./scripts/build-windows.sh

# 复用现有的同版本 SDK，跳过对应 SDK 的自动下载。
QT_HOST_PATH=/path/to/Qt/6.8.3/gcc_64 \
QT_WINDOWS_PREFIX=/path/to/Qt/6.8.3/mingw_64 \
./scripts/build-windows.sh
```

自备 SDK 必须为 Qt 6.8.3，且含上述模块；Windows SDK 必须为 MinGW x64 版本。若编译器的 DLL 位于非标准目录，通过 `MINGW_RUNTIME_DIR` 指定，使用与当前 MinGW 编译器配套的运行库。

下载中断后可直接重新运行。更换 SDK 或编译器后，请使用新的 `BUILD_DIR`，避免复用旧的 CMake 缓存。运行 `./scripts/build-windows.sh --help` 可查看所有环境变量。
