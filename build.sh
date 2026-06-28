#!/usr/bin/env bash
# MiniShot build driver.
#   ./build.sh fetch    brew deps + download third-party headers and the icon font
#   ./build.sh build    configure + build everything
#   ./build.sh test     build + run unit tests
#   ./build.sh bundle   build + assemble MiniShot.app
#   ./build.sh install  bundle + copy MiniShot.app into /Applications
#   ./build.sh run      build + run the binary
#   ./build.sh run-in-background   build + run the binary detached via nohup
#   ./build.sh clean    remove the build directory and MiniShot.app
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
PREFIX="$(brew --prefix)"

# Third-party sources, pinned by commit; downloaded, never committed.
# To upgrade, bump the pin on the matching line.
CLAY_SHA="139802baaa144c3a674db72efc25685f68fe391f"
STB_SHA="f056911"
LUCIDE_VERSION="1.21.0"

CLAY_URL="https://raw.githubusercontent.com/nicbarker/clay/$CLAY_SHA/clay.h"
STB_IMAGE_URL="https://raw.githubusercontent.com/nothings/stb/$STB_SHA/stb_image.h"
LUCIDE_URL="https://unpkg.com/lucide-static@$LUCIDE_VERSION/font/lucide.ttf"

# Install the Homebrew library dependencies, skipping any already installed.
deps() {
    for pkg in sdl3 sdl3_ttf; do
        if brew list --formula "$pkg" >/dev/null 2>&1; then
            echo "have $pkg"
        else
            brew install "$pkg"
        fi
    done
}

# Download to $1 from $2 only when absent.
fetch_one() {
    if [ -f "$1" ]; then
        echo "have $(basename "$1")"
    else
        echo "fetch $(basename "$1")"
        curl -fsSL -o "$1" "$2"
    fi
}

fetch() {
    deps
    mkdir -p "$ROOT/assets"
    fetch_one "$ROOT/3rd/clay/clay.h" "$CLAY_URL"
    fetch_one "$ROOT/3rd/stb/stb_image.h" "$STB_IMAGE_URL"
    fetch_one "$ROOT/assets/lucide.ttf" "$LUCIDE_URL"
}

# Place runtime assets next to the dev binary so SDL_GetBasePath() resolves them.
stage_assets() {
    cp "$ROOT/assets/lucide.ttf" "$BUILD/src/app/lucide.ttf"
}

configure() {
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_PREFIX_PATH="$PREFIX" >/dev/null
}

# Assemble and sign $BUILD/MiniShot.app.
bundle() {
    fetch
    configure
    cmake --build "$BUILD" --target minishot
    local app="$BUILD/MiniShot.app"
    mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
    cp "$ROOT/app_bundle/Info.plist" "$app/Contents/Info.plist"
    cp "$BUILD/src/app/minishot" "$app/Contents/MacOS/minishot"
    cp "$ROOT/assets/lucide.ttf" "$app/Contents/Resources/lucide.ttf"
    cp "$ROOT/app_bundle/MiniShot.icns" "$app/Contents/Resources/MiniShot.icns"
    codesign --force --sign - "$app"
    echo "built $app"
}

case "${1:-build}" in
    fetch)
        fetch
        ;;
    build)
        fetch
        configure
        cmake --build "$BUILD"
        stage_assets
        ;;
    test)
        fetch
        configure
        cmake --build "$BUILD"
        ctest --test-dir "$BUILD" --output-on-failure
        ;;
    bundle)
        bundle
        ;;
    install)
        bundle
        rm -rf "/Applications/MiniShot.app"
        cp -R "$BUILD/MiniShot.app" "/Applications/MiniShot.app"
        echo "installed /Applications/MiniShot.app"
        ;;
    run)
        fetch
        configure
        cmake --build "$BUILD" --target minishot
        stage_assets
        "$BUILD/src/app/minishot"
        ;;
    run-in-background)
        fetch
        configure
        cmake --build "$BUILD" --target minishot
        stage_assets
        nohup "$BUILD/src/app/minishot" >/dev/null 2>&1 &
        echo "minishot running detached (pid $!)"
        ;;
    clean)
        rm -rf "$BUILD"
        ;;
    *)
        echo "usage: $0 {fetch|build|test|bundle|install|run|run-in-background|clean}" >&2
        exit 1
        ;;
esac
