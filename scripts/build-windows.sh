#!/usr/bin/env bash
set -Eeuo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

usage() {
    cat <<'EOF'
用法：./scripts/build-windows.sh

在 Linux x86_64 上通过 Wine/MSVC 编译并打包 Windows x64 Release 版本。
首次运行下载 Qt 6.8.3 和 MSVC/Windows SDK，之后复用缓存。
MSVC 首次安装需要接受微软许可；不需要 Windows 或管理员权限。

可选环境变量：
  JOBS                  编译并行数（默认 2）
  BUILD_DIR             构建与打包目录（默认 <项目>/build/windows-msvc-x86_64）
  WINDOWS_BUILD_CACHE   SDK 和工具缓存（默认 $XDG_CACHE_HOME/gs232b-windows，
                        未设置 XDG_CACHE_HOME 时使用 ~/.cache）
  QT_MIRROR             Qt 下载镜像根地址（默认 https://mirrors.aliyun.com/qt）
  QT_HOST_PATH          已安装的 Linux Qt 6.8.3 前缀（需 CoreTools、LinguistTools）
  QT_WINDOWS_PREFIX     已安装的 Windows Qt 6.8.3 msvc2022_64 前缀
  MSVC_ROOT             已通过 msvc-wine 安装的 MSVC 前缀
  MSVC_REDIST_DIR       配套 x64 Microsoft.VC143.CRT 目录（默认从 MSVC_ROOT 探测）
  MSVC_ACCEPT_LICENSE   设为 1 表示已阅读并接受微软许可，允许非交互安装

依赖与输出说明：docs/build-windows.md
EOF
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    usage
    exit 0
fi
if (( $# != 0 )); then
    usage >&2
    exit 2
fi

fail() { printf '错误：%s\n' "$*" >&2; exit 1; }
trap 'printf "Windows 构建失败（第 %s 行）。\n" "$LINENO" >&2' ERR

[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || fail '此脚本支持 Linux x86_64 主机。'
for command_name in cmake ninja python3 wine winepath objdump; do
    command -v "$command_name" >/dev/null || fail "缺少 $command_name；请参阅 docs/build-windows.md。"
done

qt_version=6.8.3
aqt_version=3.3.0
cache_dir=${WINDOWS_BUILD_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/gs232b-windows}
build_dir=${BUILD_DIR:-$project_dir/build/windows-msvc-x86_64}
jobs=${JOBS:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || fail 'JOBS 必须是正整数。'
mkdir -p -- "$cache_dir" "$build_dir"
cache_dir=$(cd -- "$cache_dir" && pwd)
build_dir=$(cd -- "$build_dir" && pwd)

# NOTE: 旧 MinGW 缓存不能切换编译器；下载 SDK 前先阻止误用已有构建目录。
if [[ -f $build_dir/CMakeCache.txt ]]; then
    cached_toolchain=$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:[^=]*=//p' "$build_dir/CMakeCache.txt")
    [[ $cached_toolchain == "$project_dir/cmake/windows-msvc.cmake" ]] || \
        fail 'BUILD_DIR 已含其他工具链缓存；请使用新的构建目录。'
fi

qt_dir=$cache_dir/Qt
qt_host=${QT_HOST_PATH:-$qt_dir/$qt_version/gcc_64}
qt_windows=${QT_WINDOWS_PREFIX:-$qt_dir/$qt_version/msvc2022_64}
msvc_root=${MSVC_ROOT:-$cache_dir/msvc2022}

# NOTE: 编译、部署工具共用独立 Wine 前缀，不受用户原有 Wine 环境影响。
export WINEPREFIX=$cache_dir/wine-msvc
export WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml='
if [[ -z ${MSVC_ROOT:-} && ! -f $msvc_root/.gs232b-msvc-ready ]]; then
    "$project_dir/scripts/prepare-windows-msvc.sh" "$cache_dir" "$msvc_root"
fi
for tool in cl link lib rc mt; do
    [[ -x $msvc_root/bin/x64/$tool ]] || fail "MSVC 缺少 $tool 包装器：$msvc_root"
done
msvc_root=$(cd -- "$msvc_root" && pwd)
export MSVC_ROOT=$msvc_root
export PATH="$msvc_root/bin/x64:$PATH"

prepare_downloader() {
    local venv_dir=$cache_dir/aqt-venv
    if [[ ! -x $venv_dir/bin/python ]]; then
        python3 -m venv "$venv_dir"
    fi
    aqt_python=$venv_dir/bin/python
    if ! "$aqt_python" -c 'import importlib.metadata, sys; sys.exit(importlib.metadata.version("aqtinstall") != sys.argv[1])' \
        "$aqt_version" 2>/dev/null; then
        "$aqt_python" -m pip install --disable-pip-version-check "aqtinstall==$aqt_version"
    fi
}

install_qt() {
    local host=$1 arch=$2 prefix=$3
    shift 3
    prepare_downloader
    "$aqt_python" -m aqt install-qt "$host" desktop "$qt_version" "$arch" \
        --outputdir "$qt_dir" --base "${QT_MIRROR:-https://mirrors.aliyun.com/qt}" "$@"
    touch -- "$prefix/.gs232b-sdk-ready"
}

# NOTE: Linux 工具使用 SDK 配套的 ICU；Arch 系统 ICU 的主版本可能不同。
if [[ -z ${QT_HOST_PATH:-} && ( ! -f $qt_host/.gs232b-sdk-ready || ! -f $qt_host/lib/libicui18n.so.73 ) ]]; then
    install_qt linux linux_gcc_64 "$qt_host" --archives qtbase qttools icu
fi
if [[ -z ${QT_WINDOWS_PREFIX:-} && ( ! -f $qt_windows/.gs232b-sdk-ready || ! -f $qt_windows/lib/cmake/Qt6Pdf/Qt6PdfConfig.cmake ) ]]; then
    install_qt windows win64_msvc2022_64 "$qt_windows" --archives qtbase qttools qttranslations \
        --modules qtserialport qtwebsockets qtpdf
fi

for config in Qt6CoreTools Qt6LinguistTools; do
    [[ -f $qt_host/lib/cmake/$config/${config}Config.cmake ]] || fail "Linux Qt 缺少 $config：$qt_host"
done
for config in Qt6Core Qt6Widgets Qt6Network Qt6Pdf Qt6SerialPort Qt6WebSockets Qt6LinguistTools; do
    [[ -f $qt_windows/lib/cmake/$config/${config}Config.cmake ]] || fail "Windows Qt 缺少 $config：$qt_windows"
done
for tool in windeployqt qtpaths6; do
    [[ -f $qt_windows/bin/$tool.exe ]] || fail "Windows Qt 缺少 $tool.exe：$qt_windows"
done
[[ -f $qt_windows/lib/Qt6Core.lib ]] || fail 'Windows Qt 必须是 msvc2022_64 SDK，不能使用 MinGW SDK。'
grep -Fq "set(PACKAGE_VERSION \"$qt_version\")" \
    "$qt_windows/lib/cmake/Qt6Core/Qt6CoreConfigVersionImpl.cmake" || fail "Windows Qt 必须为 $qt_version。"
qt_host=$(cd -- "$qt_host" && pwd)
qt_windows=$(cd -- "$qt_windows" && pwd)
[[ $("$qt_host/bin/qmake" -query QT_VERSION) == "$qt_version" ]] || fail "Linux Qt 必须为 $qt_version，且构建工具必须能正常运行。"
"$qt_host/bin/lrelease" -version
python3 "$project_dir/scripts/verify-windows-runtime.py" "$qt_windows/bin/Qt6Core.dll" \
    "$qt_windows/plugins/platforms/qwindows.dll"

printf '配置 Windows x64 MSVC Release：%s\n' "$build_dir"
cmake -S "$project_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH="$project_dir/cmake/windows-msvc.cmake" \
    -DMSVC_ROOT="$msvc_root" \
    -DQT_WINDOWS_PREFIX="$qt_windows" \
    -DCMAKE_PREFIX_PATH="$qt_windows" \
    -DQT_HOST_PATH="$qt_host" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_WIN32_EXECUTABLE=ON \
    -DBUILD_TESTING=OFF
cmake --build "$build_dir" --target gs_232b_open_gui --parallel "$jobs"
python3 "$project_dir/scripts/verify-windows-runtime.py" "$build_dir/gs_232b_open_gui.exe"

"$project_dir/scripts/deploy-windows.sh" "$qt_windows" "$build_dir" "$cache_dir"
