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

    TEST_REPORT();
}
