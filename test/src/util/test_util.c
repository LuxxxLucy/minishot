#include <string.h>
#include "test.h"
#include "util.h"

int main(void)
{
    char buf[64];

    // ms_join_path: dir without trailing slash.
    int r = ms_join_path(buf, sizeof buf, "/tmp", "a.png");
    CHECK_STR(buf, "/tmp/a.png");
    CHECK_EQ(r, 10);

    // dir with trailing slash — do not double it.
    r = ms_join_path(buf, sizeof buf, "/tmp/", "a.png");
    CHECK_STR(buf, "/tmp/a.png");
    CHECK_EQ(r, 10);

    // empty dir — just the name, no leading slash.
    r = ms_join_path(buf, sizeof buf, "", "a.png");
    CHECK_STR(buf, "a.png");
    CHECK_EQ(r, 5);

    // join buffer one too small (10 chars + NUL needs 11).
    r = ms_join_path(buf, 10, "/tmp", "a.png");
    CHECK_EQ(r, -1);

    // join buffer exactly large enough.
    r = ms_join_path(buf, 11, "/tmp", "a.png");
    CHECK_EQ(r, 10);
    CHECK_STR(buf, "/tmp/a.png");

    // ms_expand_path_from_home: leading '~' becomes home, rest kept.
    r = ms_expand_path_from_home(buf, sizeof buf, "~/Downloads", "/Users/x");
    CHECK_STR(buf, "/Users/x/Downloads");
    CHECK_EQ(r, 18);

    // a bare '~' expands to home alone.
    r = ms_expand_path_from_home(buf, sizeof buf, "~", "/home/u");
    CHECK_STR(buf, "/home/u");
    CHECK_EQ(r, 7);

    // no leading '~' — copied unchanged.
    r = ms_expand_path_from_home(buf, sizeof buf, "/abs/path", "/home/u");
    CHECK_STR(buf, "/abs/path");
    CHECK_EQ(r, 9);

    // NULL home — '~' left untouched.
    r = ms_expand_path_from_home(buf, sizeof buf, "~/d", NULL);
    CHECK_STR(buf, "~/d");
    CHECK_EQ(r, 3);

    // buffer too small for the expansion.
    r = ms_expand_path_from_home(buf, 5, "~/Downloads", "/Users/x");
    CHECK_EQ(r, -1);

    TEST_REPORT();
}
