#!/usr/bin/env bash
# Local development build for RePrism Launcher using Homebrew-provided Qt and deps.
# Usage: scripts/dev-build.sh [run]
#   MSA_CLIENT_ID / CURSEFORGE_API_KEY env vars are passed through if set.
set -euo pipefail
cd "$(dirname "$0")/.."

BREW="$(brew --prefix)"
MESA="$(brew --prefix mesa)"
export JAVA_HOME="$(brew --prefix openjdk@17)"
export PATH="$JAVA_HOME/bin:$PATH"

if [ ! -f build/build.ninja ]; then
    cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_PREFIX_PATH="$BREW;$MESA" \
        -DOpenGL_GL_PREFERENCE=LEGACY \
        -DOPENGL_INCLUDE_DIR="$MESA/include" \
        -DOPENGL_gl_LIBRARY="$MESA/lib/libGL.so" \
        -DCMAKE_INSTALL_PREFIX="$PWD/install" \
        -DENABLE_LTO=OFF \
        -DBUILD_TESTING=OFF \
        -DLauncher_MSA_CLIENT_ID="${MSA_CLIENT_ID:-}" \
        -DLauncher_CURSEFORGE_API_KEY="${CURSEFORGE_API_KEY:-}"
fi

cmake --build build -j"$(nproc)"

if [ "${1:-}" = "run" ]; then
    # Separate data dir so dev runs never touch a real install.
    exec ./build/reprismlauncher -d "$PWD/.devdata"
fi
