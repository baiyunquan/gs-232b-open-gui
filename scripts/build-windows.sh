#!/usr/bin/env bash
set -Eeuo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

usage() {
    cat <<'EOF'
用法：./scripts/build-windows.sh

在 Linux x86_64 上交叉编译并打包 Windows x64 Release 版本。
首次运行自动下载 Qt 6.8.3，之后复用缓存；不需要 Windows 或管理员权限。

可选环境变量：
  JOBS                  编译并行数（默认 2）
  BUILD_DIR             构建与打包目录（默认 <项目>/build/windows-x86_64）
  WINDOWS_BUILD_CACHE   SDK 和工具缓存（默认 $XDG_CACHE_HOME/gs232b-windows，
                        未设置 XDG_CACHE_HOME 时使用 ~/.cache）
  QT_MIRROR             Qt 下载镜像根地址（默认 https://mirrors.aliyun.com/qt）
  QT_HOST_PATH          已安装的 Linux Qt 6.8.3 前缀（需 CoreTools、LinguistTools）
  QT_WINDOWS_PREFIX     已安装的 Windows Qt 6.8.3 MinGW 前缀
  MINGW_RUNTIME_DIR     MinGW 运行时 DLL 目录（默认从编译器路径探测）

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
for command_name in cmake ninja python3 wine winepath x86_64-w64-mingw32-gcc \
    x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres; do
    command -v "$command_name" >/dev/null || fail "缺少 $command_name；请参阅 docs/build-windows.md。"
done

qt_version=6.8.3
aqt_version=3.3.0
cache_dir=${WINDOWS_BUILD_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/gs232b-windows}
build_dir=${BUILD_DIR:-$project_dir/build/windows-x86_64}
jobs=${JOBS:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || fail 'JOBS 必须是正整数。'
mkdir -p -- "$cache_dir" "$build_dir"
cache_dir=$(cd -- "$cache_dir" && pwd)
build_dir=$(cd -- "$build_dir" && pwd)
qt_dir=$cache_dir/Qt
qt_host=${QT_HOST_PATH:-$qt_dir/$qt_version/gcc_64}
qt_windows=${QT_WINDOWS_PREFIX:-$qt_dir/$qt_version/mingw_64}

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
if [[ -z ${QT_WINDOWS_PREFIX:-} && ! -f $qt_windows/.gs232b-sdk-ready ]]; then
    install_qt windows win64_mingw "$qt_windows" --archives qtbase qttools qttranslations \
        --modules qtserialport qtwebsockets
fi

for config in Qt6CoreTools Qt6LinguistTools; do
    [[ -f $qt_host/lib/cmake/$config/${config}Config.cmake ]] || fail "Linux Qt 缺少 $config：$qt_host"
done
for config in Qt6Core Qt6Widgets Qt6SerialPort Qt6WebSockets Qt6LinguistTools; do
    [[ -f $qt_windows/lib/cmake/$config/${config}Config.cmake ]] || fail "Windows Qt 缺少 $config：$qt_windows"
done
[[ -f $qt_windows/bin/windeployqt.exe ]] || fail "Windows Qt 缺少 windeployqt.exe：$qt_windows"
qt_host=$(cd -- "$qt_host" && pwd)
qt_windows=$(cd -- "$qt_windows" && pwd)
[[ $("$qt_host/bin/qmake" -query QT_VERSION) == "$qt_version" ]] || fail "Linux Qt 必须为 $qt_version，且构建工具必须能正常运行。"
"$qt_host/bin/lrelease" -version

printf '配置 Windows x64 Release：%s\n' "$build_dir"
# NOTE: MinGW 直接使用 main 入口；仅由链接器选择 GUI 子系统，避开旧 Qt 的 MSVCRT 入口包装。
cmake -S "$project_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$project_dir/cmake/windows-mingw.cmake" \
    -DQT_WINDOWS_PREFIX="$qt_windows" \
    -DCMAKE_PREFIX_PATH="$qt_windows" \
    -DQT_HOST_PATH="$qt_host" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_WIN32_EXECUTABLE=OFF \
    -DCMAKE_EXE_LINKER_FLAGS=-mwindows \
    -DBUILD_TESTING=OFF
cmake --build "$build_dir" --target gs_232b_open_gui --parallel "$jobs"

"$project_dir/scripts/deploy-windows.sh" "$qt_windows" "$build_dir" "$cache_dir"
