#ifndef MINISHOT_CONFIG_H
#define MINISHOT_CONFIG_H

#include <stddef.h>

struct ms_config {
    char save_dir[1024];   // where quick-save writes; default "~/Downloads"
    char hotkey[64];       // capture hotkey, e.g. "cmd+shift+a"
    char font_path[1024];  // UI text font; empty means use the app default
};

// Defaults: save_dir = "~/Downloads", hotkey = "cmd+shift+a", font_path = "".
struct ms_config ms_config_default(void);

// Parse "key=value" lines (keys: save_dir, hotkey) over the given cfg.
// Blank lines and lines starting with '#' are ignored.
// Returns 0 on success, -1 on a malformed line.
int ms_config_parse(struct ms_config *cfg, const char *text);

// Serialize cfg as "key=value" lines into out.
// Returns chars written excluding the NUL, or -1 if the buffer is too small.
int ms_config_serialize(const struct ms_config *cfg, char *out, size_t n);

#endif
