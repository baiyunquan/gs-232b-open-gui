#!/usr/bin/env bash
set -Eeuo pipefail

# NOTE: 内部安装入口；许可证由下载器交互确认，或由调用者明确设置接受标志。
[[ $# == 2 ]] || { printf '用法：%s <缓存目录> <MSVC 安装目录>\n' "$0" >&2; exit 2; }
cache_dir=$1
msvc_root=$2
revision=5dd115047b7ddcbb53cbc8b5b14163a20f4334fe
archive_hash=362455702f6b01feebd47afc21752590f5dfd57790d6eec632cff7f235fcad32
source_dir=$cache_dir/msvc-wine-$revision
archive=$cache_dir/downloads/msvc-wine-$revision.tar.gz

for command_name in curl tar sha256sum msiextract; do
    command -v "$command_name" >/dev/null || { printf '缺少 %s；请参阅 docs/build-windows.md。\n' "$command_name" >&2; exit 1; }
done
mkdir -p -- "$cache_dir/downloads"
if [[ ! -f $archive ]]; then
    curl --fail --location --retry 3 --output "$archive.part" \
        "https://api.github.com/repos/mstorsjo/msvc-wine/tarball/$revision"
    mv -- "$archive.part" "$archive"
fi
printf '%s  %s\n' "$archive_hash" "$archive" | sha256sum --check --status
mkdir -p -- "$source_dir"
tar -xzf "$archive" --strip-components=1 -C "$source_dir"

license_args=()
if [[ ${MSVC_ACCEPT_LICENSE:-0} == 1 ]]; then
    license_args+=(--accept-license)
elif [[ ! -t 0 ]]; then
    printf '首次安装需接受微软许可。请交互运行，或阅读许可后设置 MSVC_ACCEPT_LICENSE=1：\nhttps://go.microsoft.com/fwlink/?LinkId=2086102\n' >&2
    exit 1
fi
python3 "$source_dir/vsdownload.py" --major 17 --msvc-version 14.44 \
    --sdk-version 10.0.26100 --architecture x64 --host-arch x64 \
    --with-asan no --with-atl no --with-dia no --with-msbuild no --with-devcmd no \
    "${license_args[@]}" --dest "$msvc_root" --cache "$cache_dir/vs-downloads"
"$source_dir/install.sh" "$msvc_root"
touch -- "$msvc_root/.gs232b-msvc-ready"
