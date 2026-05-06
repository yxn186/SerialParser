#!/usr/bin/env bash
set -euo pipefail

# Linux AppImage 打包脚本。
# 依赖 linuxdeployqt AppImage，默认路径为项目根目录 linuxdeployqt-continuous-x86_64.AppImage。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-build_linux}"
APPDIR="${APPDIR:-AppDir}"
OUTPUT_NAME="${OUTPUT_NAME:-SerialParser-Linux-x86_64.AppImage}"
QT_PREFIX="${QT_PREFIX:-${CMAKE_PREFIX_PATH:-$HOME/Qt/6.11.0/gcc_64}}"
LINUXDEPLOYQT="${LINUXDEPLOYQT:-$ROOT_DIR/linuxdeployqt-continuous-x86_64.AppImage}"

APP_BINARY="$ROOT_DIR/$BUILD_DIR/SerialParserApp"

if [[ ! -x "$APP_BINARY" ]]; then
    echo "错误：找不到可执行文件 $APP_BINARY，请先运行 scripts/build_linux.sh"
    exit 1
fi

if [[ ! -x "$LINUXDEPLOYQT" ]]; then
    echo "错误：找不到可执行 linuxdeployqt：$LINUXDEPLOYQT"
    echo "可从 https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage 下载。"
    exit 1
fi

cd "$ROOT_DIR"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$APP_BINARY" "$APPDIR/usr/bin/"
cp -r "$ROOT_DIR/$BUILD_DIR/configs" "$APPDIR/usr/bin/"
cp -r "$ROOT_DIR/$BUILD_DIR/styles" "$APPDIR/usr/bin/"
cp -r "$ROOT_DIR/$BUILD_DIR/resources" "$APPDIR/usr/bin/"
cp "$ROOT_DIR/$BUILD_DIR/README.md" "$APPDIR/usr/bin/"
cp "$ROOT_DIR/resources/app_icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/serialparser.png"

cat > "$APPDIR/usr/share/applications/serialparser.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=SerialParser
Comment=STM32 Serial Protocol Parser
Exec=SerialParserApp
Icon=serialparser
Categories=Development;Utility;
Terminal=false
EOF

export PATH="$QT_PREFIX/bin:$PATH"
export QMAKE="${QMAKE:-$QT_PREFIX/bin/qmake}"

rm -f "$OUTPUT_NAME"

"$LINUXDEPLOYQT" \
    "$APPDIR/usr/share/applications/serialparser.desktop" \
    -appimage \
    -bundle-non-qt-libs \
    -unsupported-allow-new-glibc

APPIMAGE_PATH="$(find "$ROOT_DIR" -maxdepth 1 -type f -name '*.AppImage' ! -name 'linuxdeployqt*' | head -n 1)"

if [[ -z "$APPIMAGE_PATH" ]]; then
    echo "错误：linuxdeployqt 未生成 AppImage。"
    exit 1
fi

mv "$APPIMAGE_PATH" "$ROOT_DIR/$OUTPUT_NAME"
chmod +x "$ROOT_DIR/$OUTPUT_NAME"

echo "Linux AppImage 已生成：$ROOT_DIR/$OUTPUT_NAME"
