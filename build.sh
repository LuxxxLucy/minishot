#!/usr/bin/env bash
# MiniShot build driver.
#   ./build.sh build    configure + build everything
#   ./build.sh test     build + run unit tests
#   ./build.sh bundle   build + assemble MiniShot.app
#   ./build.sh run      build + run the binary
#   ./build.sh clean    remove the build directory and MiniShot.app
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
PREFIX="$(brew --prefix)"

# Bundled assets are downloaded, never committed (see .gitignore).
LUCIDE_URL="https://unpkg.com/lucide-static@1.21.0/font/lucide.ttf"

fetch_assets() {
    mkdir -p "$ROOT/assets"
    [ -f "$ROOT/assets/lucide.ttf" ] || curl -fsSL -o "$ROOT/assets/lucide.ttf" "$LUCIDE_URL"
}

# Place runtime assets next to the dev binary so SDL_GetBasePath() resolves them.
stage_assets() {
    cp "$ROOT/assets/lucide.ttf" "$BUILD/src/app/lucide.ttf"
}

configure() {
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_PREFIX_PATH="$PREFIX" >/dev/null
}

case "${1:-build}" in
    build)
        fetch_assets
        configure
        cmake --build "$BUILD"
        stage_assets
        ;;
    test)
        configure
        cmake --build "$BUILD"
        ctest --test-dir "$BUILD" --output-on-failure
        ;;
    bundle)
        fetch_assets
        configure
        cmake --build "$BUILD" --target minishot
        APP="$BUILD/MiniShot.app"
        mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
        cp "$ROOT/app_bundle/Info.plist" "$APP/Contents/Info.plist"
        cp "$BUILD/src/app/minishot" "$APP/Contents/MacOS/minishot"
        cp "$ROOT/assets/lucide.ttf" "$APP/Contents/Resources/lucide.ttf"
        cp "$ROOT/app_bundle/MiniShot.icns" "$APP/Contents/Resources/MiniShot.icns"
        codesign --force --sign - "$APP"
        echo "built $APP"
        ;;
    run)
        fetch_assets
        configure
        cmake --build "$BUILD" --target minishot
        stage_assets
        "$BUILD/src/app/minishot"
        ;;
    clean)
        rm -rf "$BUILD"
        ;;
    *)
        echo "usage: $0 {build|test|bundle|run|clean}" >&2
        exit 1
        ;;
esac
