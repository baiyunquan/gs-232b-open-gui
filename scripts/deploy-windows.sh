#!/usr/bin/env bash
set -Eeuo pipefail

# NOTE: 内部打包入口，由 build-windows.sh 传入 Qt 前缀、构建目录、缓存目录。
[[ $# == 3 ]] || { printf '用法：%s <Windows Qt 前缀> <构建目录> <缓存目录>\n' "$0" >&2; exit 2; }
qt_windows=$1
build_dir=$2
cache_dir=$3
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
package_name=gs-232b-open-gui-windows-x86_64
staging_dir=$(mktemp -d "$build_dir/.windows-package.XXXXXX")
trap 'rm -rf -- "$staging_dir"' EXIT
package_dir=$staging_dir/$package_name
mkdir -p -- "$package_dir"
cp -- "$build_dir/gs_232b_open_gui.exe" "$package_dir/"

# NOTE: 使用实际交叉编译器的运行库，避免 Qt SDK 自带旧版 libstdc++ 缺少符号。
mingw_import_lib=$(x86_64-w64-mingw32-gcc -print-file-name=libkernel32.a)
runtime_dir=${MINGW_RUNTIME_DIR:-$(dirname -- "$mingw_import_lib")/../bin}
for dll in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
    runtime=$(x86_64-w64-mingw32-g++ -print-file-name="$dll")
    if [[ -n ${MINGW_RUNTIME_DIR:-} || ! -f $runtime ]]; then
        runtime=$runtime_dir/$dll
    fi
    if [[ ! -f $runtime ]]; then
        printf '找不到 %s；请设置 MINGW_RUNTIME_DIR 为当前 MinGW 的 DLL 目录。\n' "$dll" >&2
        exit 1
    fi
    cp -- "$runtime" "$package_dir/$dll"
done

# NOTE: 独立 Wine 前缀只用于执行部署工具，不改动用户已有的 Wine 环境。
export WINEPREFIX=$cache_dir/wine
export WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml='
package_windows=$(winepath -w "$package_dir")
qt_bin_windows=$(winepath -w "$qt_windows/bin")
export WINEPATH="$package_windows;$qt_bin_windows"
wine "$qt_windows/bin/windeployqt.exe" --release --no-compiler-runtime \
    --no-opengl-sw --no-system-d3d-compiler --no-translations \
    --dir "$package_windows" "$package_windows\\gs_232b_open_gui.exe"

cat > "$package_dir/qt.conf" <<'EOF'
[Paths]
Prefix=.
Plugins=.
Translations=translations
EOF
# NOTE: main.cpp 按 qtbase 名称加载翻译；windeployqt 默认合并成 qt_zh_CN，名称不匹配。
mkdir -p -- "$package_dir/translations"
cp -- "$qt_windows/translations/qtbase_zh_CN.qm" "$package_dir/translations/"
cp -- "$project_dir/LICENSE" "$package_dir/"
cp -- "$project_dir/docs/virtual-serial.md" "$package_dir/"
cp -- "$project_dir/docs/terminal.md" "$package_dir/"
cp -R -- "$build_dir/licenses" "$package_dir/"
for license_dir in LICENSES licenses; do
    if [[ -d $qt_windows/$license_dir ]]; then
        cp -R -- "$qt_windows/$license_dir" "$package_dir/qt-licenses"
        break
    fi
done

python3 - "$staging_dir" "$package_name" "$build_dir" <<'PY'
import pathlib
import shutil
import sys

staging, name, output = sys.argv[1:]
shutil.make_archive(str(pathlib.Path(output) / name), "zip", staging, name)
PY
# NOTE: 仅替换固定的生成目录；打包失败时保留上一次成功的 package。
rm -rf -- "$build_dir/package"
mv -- "$package_dir" "$build_dir/package"
printf '\nWindows 程序目录：%s/package\nWindows ZIP 包：%s/%s.zip\n' "$build_dir" "$build_dir" "$package_name"
