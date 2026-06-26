#include <string.h>
#include "test.h"
#include "config.h"

static void test_defaults(void)
{
    struct ms_config cfg = ms_config_default();
    CHECK_STR(cfg.save_dir, "~/Downloads");
    CHECK_STR(cfg.hotkey, "cmd+shift+a");
    CHECK_STR(cfg.font_dir, "/System/Library/Fonts");
    CHECK_STR(cfg.font_name, "SFNS.ttf");
    CHECK_STR(cfg.log_path, "/tmp/minishot.log");
    CHECK_STR(cfg.temp_dir, "/tmp");
}

static void test_font_fields(void)
{
    struct ms_config cfg = ms_config_default();
    int rc = ms_config_parse(&cfg, "font_dir=/Library/Fonts\nfont_name=My.ttf\n");
    CHECK_EQ(rc, 0);
    CHECK_STR(cfg.font_dir, "/Library/Fonts");
    CHECK_STR(cfg.font_name, "My.ttf");

    // The font fields appear in the serialized form and round-trip.
    char out[256];
    int n = ms_config_serialize(&cfg, out, sizeof(out));
    CHECK(n > 0);
    struct ms_config fresh = ms_config_default();
    CHECK_EQ(ms_config_parse(&fresh, out), 0);
    CHECK_STR(fresh.font_dir, "/Library/Fonts");
    CHECK_STR(fresh.font_name, "My.ttf");
}

static void test_normal_parse(void)
{
    struct ms_config cfg = ms_config_default();
    int rc = ms_config_parse(&cfg, "save_dir=/tmp/shots\nhotkey=cmd+1\n");
    CHECK_EQ(rc, 0);
    CHECK_STR(cfg.save_dir, "/tmp/shots");
    CHECK_STR(cfg.hotkey, "cmd+1");
}

static void test_missing_trailing_newline(void)
{
    struct ms_config cfg = ms_config_default();
    int rc = ms_config_parse(&cfg, "save_dir=/a\nhotkey=cmd+2");
    CHECK_EQ(rc, 0);
    CHECK_STR(cfg.save_dir, "/a");
    CHECK_STR(cfg.hotkey, "cmd+2");
}

static void test_whitespace_trimming(void)
{
    struct ms_config cfg = ms_config_default();
    int rc = ms_config_parse(
        &cfg, "  save_dir  =   /trim/me   \n\thotkey\t=\tcmd+x\t\n");
    CHECK_EQ(rc, 0);
    CHECK_STR(cfg.save_dir, "/trim/me");
    CHECK_STR(cfg.hotkey, "cmd+x");
}

static void test_comment_and_blank_skipping(void)
{
    struct ms_config cfg = ms_config_default();
    int rc = ms_config_parse(&cfg,
                             "\n"
                             "# a comment\n"
                             "   # indented comment\n"
                             "\n"
                             "save_dir=/kept\n"
                             "   \n");
    CHECK_EQ(rc, 0);
    CHECK_STR(cfg.save_dir, "/kept");
    CHECK_STR(cfg.hotkey, "cmd+shift+a");  // untouched default
}

static void test_unknown_key_ignored(void)
{
    struct ms_config cfg = ms_config_default();
    int rc = ms_config_parse(&cfg, "bogus=value\nsave_dir=/ok\n");
    CHECK_EQ(rc, 0);
    CHECK_STR(cfg.save_dir, "/ok");
    CHECK_STR(cfg.hotkey, "cmd+shift+a");
}

static void test_malformed_line(void)
{
    struct ms_config cfg = ms_config_default();
    int rc =
        ms_config_parse(&cfg, "save_dir=/ok\nno_equals_here\nhotkey=cmd+z\n");
    CHECK_EQ(rc, -1);
    // parsing stops at the malformed line; earlier line applied, later not
    CHECK_STR(cfg.save_dir, "/ok");
    CHECK_STR(cfg.hotkey, "cmd+shift+a");
}

static void test_truncation(void)
{
    struct ms_config cfg = ms_config_default();
    char line[64 + 1100];
    char big[1100];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    snprintf(line, sizeof(line), "save_dir=%s\n", big);
    int rc = ms_config_parse(&cfg, line);
    CHECK_EQ(rc, 0);
    // never overflows: stays NUL-terminated within 1024 bytes
    CHECK_EQ(strlen(cfg.save_dir), (size_t)1023);
}

static void test_serialize_exact(void)
{
    struct ms_config cfg = ms_config_default();
    strcpy(cfg.save_dir, "/foo");
    strcpy(cfg.hotkey, "cmd+y");
    char out[256];
    int n = ms_config_serialize(&cfg, out, sizeof(out));
    const char *want =
        "save_dir=/foo\n"
        "hotkey=cmd+y\n"
        "font_dir=/System/Library/Fonts\n"
        "font_name=SFNS.ttf\n"
        "log_path=/tmp/minishot.log\n"
        "temp_dir=/tmp\n";
    CHECK_STR(out, want);
    CHECK_EQ(n, (int)strlen(want));
}

static void test_serialize_buffer_too_small(void)
{
    struct ms_config cfg = ms_config_default();
    char out[4];
    int n = ms_config_serialize(&cfg, out, sizeof(out));
    CHECK_EQ(n, -1);
}

static void test_round_trip(void)
{
    struct ms_config cfg = ms_config_default();
    strcpy(cfg.save_dir, "/some/where/deep");
    strcpy(cfg.hotkey, "ctrl+alt+q");
    char out[2048];
    int n = ms_config_serialize(&cfg, out, sizeof(out));
    CHECK(n > 0);

    struct ms_config fresh = ms_config_default();
    int rc = ms_config_parse(&fresh, out);
    CHECK_EQ(rc, 0);
    CHECK_STR(fresh.save_dir, cfg.save_dir);
    CHECK_STR(fresh.hotkey, cfg.hotkey);
}

int main(void)
{
    test_defaults();
    test_font_fields();
    test_normal_parse();
    test_missing_trailing_newline();
    test_whitespace_trimming();
    test_comment_and_blank_skipping();
    test_unknown_key_ignored();
    test_malformed_line();
    test_truncation();
    test_serialize_exact();
    test_serialize_buffer_too_small();
    test_round_trip();
    TEST_REPORT();
}
