#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

ms_config ms_config_default(void)
{
    ms_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.save_dir, "~/Downloads");
    strcpy(cfg.hotkey, "cmd+shift+a");
    return cfg;
}

// Copy [begin, end) into dst, truncating to fit and NUL-terminating.
static void copy_field(char *dst, size_t cap, const char *begin,
                       const char *end)
{
    if (cap == 0) {
        return;
    }
    size_t len = (size_t)(end - begin);
    if (len > cap - 1) {
        len = cap - 1;
    }
    memcpy(dst, begin, len);
    dst[len] = '\0';
}

int ms_config_parse(ms_config *cfg, const char *text)
{
    const char *p = text;
    while (*p) {
        const char *line = p;
        const char *eol = strchr(p, '\n');
        const char *line_end;
        if (eol) {
            line_end = eol;
            p = eol + 1;
        } else {
            line_end = line + strlen(line);
            p = line_end;
        }

        // Trim leading whitespace.
        const char *s = line;
        while (s < line_end && isspace((unsigned char)*s)) {
            s++;
        }
        // Trim trailing whitespace.
        const char *e = line_end;
        while (e > s && isspace((unsigned char)*(e - 1))) {
            e--;
        }

        if (s == e) {
            continue;  // blank line
        }
        if (*s == '#') {
            continue;  // comment
        }

        const char *eq = memchr(s, '=', (size_t)(e - s));
        if (!eq) {
            return -1;  // malformed: non-blank, non-comment, no '='
        }

        // Key: [s, eq), trimmed.
        const char *ks = s;
        const char *ke = eq;
        while (ke > ks && isspace((unsigned char)*(ke - 1))) {
            ke--;
        }
        // Value: (eq, e), trimmed (leading only; trailing already trimmed on
        // e).
        const char *vs = eq + 1;
        const char *ve = e;
        while (vs < ve && isspace((unsigned char)*vs)) {
            vs++;
        }

        size_t klen = (size_t)(ke - ks);
        if (klen == strlen("save_dir") && memcmp(ks, "save_dir", klen) == 0) {
            copy_field(cfg->save_dir, sizeof(cfg->save_dir), vs, ve);
        } else if (klen == strlen("hotkey") &&
                   memcmp(ks, "hotkey", klen) == 0) {
            copy_field(cfg->hotkey, sizeof(cfg->hotkey), vs, ve);
        } else if (klen == strlen("font_path") &&
                   memcmp(ks, "font_path", klen) == 0) {
            copy_field(cfg->font_path, sizeof(cfg->font_path), vs, ve);
        }
        // Unknown keys ignored.
    }
    return 0;
}

int ms_config_serialize(const ms_config *cfg, char *out, size_t n)
{
    int written = snprintf(out, n, "save_dir=%s\nhotkey=%s\n", cfg->save_dir,
                           cfg->hotkey);
    if (written < 0 || (size_t)written >= n) {
        return -1;
    }
    // font_path is written only when set, so an unconfigured config round-trips
    // to the same two-line form.
    if (cfg->font_path[0]) {
        int more = snprintf(out + written, n - (size_t)written,
                            "font_path=%s\n", cfg->font_path);
        if (more < 0 || (size_t)(written + more) >= n) {
            return -1;
        }
        written += more;
    }
    return written;
}
