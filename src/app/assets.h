#ifndef MINISHOT_ASSETS_H
#define MINISHOT_ASSETS_H

// Asset, font, and path names. Kept here so no file or font name is hardcoded
// at a use site.
#define MS_UI_FONT_PATH "/System/Library/Fonts/SFNS.ttf"  // macOS UI font
#define MS_UI_FONT_SIZE 15.0f
#define MS_ICON_FONT_FILE "lucide.ttf"    // bundled next to the binary
#define MS_RESOURCES_DIR "../Resources/"  // app-bundle fallback location
#define MS_CONFIG_FILE ".minishot.conf"   // under $HOME

#define MS_LOG_PATH "/tmp/minishot.log"
#define MS_TMP_CAPTURE_FMT "/tmp/minishot-%llu.png"  // %llu = ticks, unique

#endif
