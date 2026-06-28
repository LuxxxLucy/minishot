# MiniShot

Minimial MacOS screenshot tool in C.
Press the capture hotkey (⇧⌘A shift+cmd+A by default), drag-select a region, pin it, resize and drag around or copy and save.

![demo](doc/demo.png)

## Quick start

```
brew install sdl3 sdl3_ttf       # the two library dependencies
./build.sh install               # fetch dependencies, build, and install into /Applications
```

## Build

```
./build.sh fetch                 # brew deps + download the headers and font
./build.sh build                 # fetch, configure, and build
./build.sh test                  # fetch, build, and run unit tests
./build.sh run                   # fetch, build, and launch the binary
./build.sh run-in-background     # fetch, build, and launch detached via nohup
./build.sh bundle                # package into MiniShot.app
./build.sh install               # bundle, then copy into /Applications
```

## Stack

- SDL3 + SDL3_ttf (window, render, text)
- Clay (layout)
- stb_image (PNG load)
- macOS `screencapture` CLI, and Carbon for the system-wide capture hotkey

## Inspirations

- [Snipaste](https://www.snipaste.com)
- [CleanShot](https://cleanshot.com)

