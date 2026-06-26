#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

struct ms_config ms_config_default(void)
{
    struct ms_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.save_dir, MS_CONFIG_DEFAULT_SAVE_DIR);
    strcpy(cfg.hotkey, MS_CONFIG_DEFAULT_HOTKEY);
    strcpy(cfg.font_dir, MS_CONFIG_DEFAULT_FONT_DIR);
    strcpy(cfg.font_name, MS_CONFIG_DEFAULT_FONT_NAME);
    strcpy(cfg.log_path, MS_CONFIG_DEFAULT_LOG_PATH);
    strcpy(cfg.temp_dir, MS_CONFIG_DEFAULT_TEMP_DIR);
    return cfg;
}

// One "key=value" field bound to its storage in struct ms_config.
struct ms_field {
    const char *key;
    size_t off;
    size_t cap;
};

#define MS_FIELD(name)                         \
    { #name, offsetof(struct ms_config, name), \
      sizeof(((struct ms_config *)0)->name) }

static const struct ms_field ms_fields[] = {
    MS_FIELD(save_dir),  MS_FIELD(hotkey),   MS_FIELD(font_dir),
    MS_FIELD(font_name), MS_FIELD(log_path), MS_FIELD(temp_dir),
};
static const size_t ms_field_count = sizeof(ms_fields) / sizeof(ms_fields[0]);

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

int ms_config_parse(struct ms_config *cfg, const char *text)
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

        // Trim leading and trailing whitespace.
        const char *s = line;
        while (s < line_end && isspace((unsigned char)*s)) {
            s++;
        }
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

        const char *ks = s;
        const char *ke = eq;
        while (ke > ks && isspace((unsigned char)*(ke - 1))) {
            ke--;
        }
        const char *vs = eq + 1;
        const char *ve = e;
        while (vs < ve && isspace((unsigned char)*vs)) {
            vs++;
        }

        size_t klen = (size_t)(ke - ks);
        for (size_t i = 0; i < ms_field_count; i++) {
            if (klen == strlen(ms_fields[i].key) &&
                memcmp(ks, ms_fields[i].key, klen) == 0) {
                copy_field((char *)cfg + ms_fields[i].off, ms_fields[i].cap, vs,
                           ve);
                break;
            }
        }
        // Unknown keys ignored.
    }
    return 0;
}

int ms_config_serialize(const struct ms_config *cfg, char *out, size_t n)
{
    int written = 0;
    for (size_t i = 0; i < ms_field_count; i++) {
        int more =
            snprintf(out + written, n - (size_t)written, "%s=%s\n",
                     ms_fields[i].key, (const char *)cfg + ms_fields[i].off);
        if (more < 0 || (size_t)(written + more) >= n) {
            return -1;
        }
        written += more;
    }
    return written;
}
