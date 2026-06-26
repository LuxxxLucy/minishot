#ifndef MINISHOT_ASSETS_H
#define MINISHOT_ASSETS_H

// Bundled-asset and app-bundle names, plus the config file path. These are not
// config fields: the config file path bootstraps config, and the rest ship with
// the app. User-tunable paths (save dir, font, log, temp) live in config.h.
#define MS_UI_FONT_SIZE 15.0f
#define MS_ICON_FONT_FILE "lucide.ttf"    // bundled next to the binary
#define MS_RESOURCES_DIR "../Resources/"  // app-bundle fallback location
#define MS_CONFIG_FILE ".minishot.conf"   // under $HOME

#define MS_TMP_CAPTURE_NAME_FMT "minishot-%llu.png"  // %llu = ticks, unique

#endif
