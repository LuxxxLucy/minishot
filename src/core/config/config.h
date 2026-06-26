#ifndef MINISHOT_CONFIG_H
#define MINISHOT_CONFIG_H

#include <stddef.h>

#define MS_CONFIG_DEFAULT_SAVE_DIR "~/Downloads"
#define MS_CONFIG_DEFAULT_HOTKEY "cmd+shift+a"
#define MS_CONFIG_DEFAULT_FONT_DIR "/System/Library/Fonts"
#define MS_CONFIG_DEFAULT_FONT_NAME "SFNS.ttf"
#define MS_CONFIG_DEFAULT_LOG_PATH "/tmp/minishot.log"
#define MS_CONFIG_DEFAULT_TEMP_DIR "/tmp"

struct ms_config {
    char save_dir[1024];  // where quick-save writes
    char hotkey[64];      // capture hotkey, e.g. "cmd+shift+a"
    char font_dir[1024];  // UI font directory
    char font_name[256];  // UI font file name
    char log_path[1024];  // diagnostic log file
    char temp_dir[1024];  // scratch dir for the intermediate capture
};

// Populate cfg with the MS_CONFIG_DEFAULT_* values.
struct ms_config ms_config_default(void);

// Parse "key=value" lines over cfg (keys: save_dir, hotkey, font_dir,
// font_name, log_path, temp_dir). Blank lines, '#' lines, and unknown keys are
// ignored. Returns 0 on success, -1 on a malformed line.
int ms_config_parse(struct ms_config *cfg, const char *text);

// Serialize cfg as "key=value" lines into out.
// Returns chars written excluding the NUL, or -1 if the buffer is too small.
int ms_config_serialize(const struct ms_config *cfg, char *out, size_t n);

#endif
