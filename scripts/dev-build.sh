#!/usr/bin/env bash
# Local development build for RePrism Launcher using Homebrew-provided Qt and deps.
# Usage: scripts/dev-build.sh [run]
#   MSA_CLIENT_ID / CURSEFORGE_API_KEY env vars override the built-in keys if set.
set -euo pipefail
cd "$(dirname "$0")/.."

BREW="$(brew --prefix)"
MESA="$(brew --prefix mesa)"
export JAVA_HOME="$(brew --prefix openjdk@17)"
export PATH="$JAVA_HOME/bin:$PATH"

# Runtime Qt plugins (SVG icons, WebP modpack icons) must exist and match qtbase's version exactly.
QT_VERSION="$(brew list --versions qtbase | awk '{print $2}')"
for pkg in qtsvg qtimageformats; do
    v="$(brew list --versions "$pkg" | awk '{print $2}')"
    if [ "$v" != "$QT_VERSION" ]; then
        echo "warning: $pkg is '${v:-not installed}' but qtbase is $QT_VERSION; icons may not load." >&2
        echo "         fix with: brew install $pkg && brew upgrade $pkg" >&2
    fi
done

KEY_ARGS=()
[ -n "${MSA_CLIENT_ID:-}" ] && KEY_ARGS+=("-DLauncher_MSA_CLIENT_ID=$MSA_CLIENT_ID")
[ -n "${CURSEFORGE_API_KEY:-}" ] && KEY_ARGS+=("-DLauncher_CURSEFORGE_API_KEY=$CURSEFORGE_API_KEY")

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
        -DLauncher_ENABLE_JAVA_DOWNLOADER=ON \
        "${KEY_ARGS[@]}"
fi

cmake --build build -j"$(nproc)"

if [ "${1:-}" = "run" ]; then
    # Separate data dir so dev runs never touch a real install.
    exec ./build/reprismlauncher -d "$PWD/.devdata"
fi
