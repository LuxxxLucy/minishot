# MiniShot

Minimal macOS screenshot + pin tool. Pure C, SDL3 + Clay, no Objective-C.

Press a hotkey, drag-select a region, then pin it as a floating image you can move and resize, copy, or save. Runs as a menu-bar agent with no Dock icon.

## Goal

The smallest possible codebase that delivers capture, pin, move, resize, copy, save, delete. Pure C is both a constraint and a goal.

## Stack

- **SDL3** — windows, tray icon, event loop, file dialog, clipboard.
- **Clay** — UI layout for the settings window, via its SDL3 renderer. The pin window draws itself directly for per-frame animation control.
- **Carbon** `RegisterEventHotKey` — global hotkey.
- **`screencapture`** CLI — pixel grab.

## Inspirations

- [Snipaste](https://www.snipaste.com) — the capture-then-pin model: a screenshot becomes a floating window you keep on screen while you work.
- [CleanShot](https://cleanshot.com) — the bar for polish: the framed capture, the quick-action toolbar, the restrained motion. MiniShot aims at that finish with a fraction of the surface.

## Build

Requires SDL3, SDL3_ttf (Homebrew). The Lucide icon font is downloaded on first build, never committed.

- `./build.sh build` — configure and build.
- `./build.sh test` — build and run unit tests.
- `./build.sh run` — build and launch the binary.
- `./build.sh bundle` — assemble `MiniShot.app` (generates the `.icns` icon).

See the design and implementation notes in the parent project folder.
