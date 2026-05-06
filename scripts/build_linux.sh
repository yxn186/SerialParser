#!/usr/bin/env bash
set -euo pipefail

# Linux 构建脚本：供 GitHub Actions、WSL 或原生 Linux 复用。
# 使用前需要准备 Linux 版 Qt，并通过 QT_PREFIX 或 CMAKE_PREFIX_PATH 指向 Qt 安装目录。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-build_linux}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
QT_PREFIX="${QT_PREFIX:-${CMAKE_PREFIX_PATH:-$HOME/Qt/6.11.0/gcc_64}}"

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"

cmake --build "$BUILD_DIR" --parallel
