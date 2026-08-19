#!/bin/sh
set -eu

version="0.7.2"
package="webrtc-streamer-v${version}-Linux-x86_64-Release"
url="https://github.com/mpromonet/webrtc-streamer/releases/download/v${version}/${package}.tar.gz"
checksum="1321ee8bba0fd29791077c6ab80100b2d8766f2fbe708f51a94418fb8bca93b5"
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
install_dir="$project_dir/.webrtc-streamer"
temp_dir=$(mktemp -d)

cleanup() {
    rm -rf "$temp_dir"
}
trap cleanup EXIT HUP INT TERM

if [ "$(uname -s)" != "Linux" ] || [ "$(uname -m)" != "x86_64" ]; then
    echo "错误: 此安装脚本仅支持 Linux x86_64" >&2
    exit 1
fi

echo "正在下载 webrtc-streamer v${version}..."
curl -fL "$url" -o "$temp_dir/package.tar.gz"
printf '%s  %s\n' "$checksum" "$temp_dir/package.tar.gz" | sha256sum -c -

mkdir "$temp_dir/package"
tar -xzf "$temp_dir/package.tar.gz" -C "$temp_dir/package" --strip-components=1
"$temp_dir/package/webrtc-streamer" -V

rm -rf "$install_dir"
mv "$temp_dir/package" "$install_dir"
echo "安装完成: $install_dir/webrtc-streamer"
