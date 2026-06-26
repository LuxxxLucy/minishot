#include <string.h>
#include "test.h"
#include "naming.h"

int main(void)
{
    char buf[64];

    // ms_build_filename: canonical case with zero-padding.
    int r = ms_build_filename(buf, sizeof buf, 2026, 6, 19, 9, 5, 3);
    CHECK_STR(buf, "minishot-20260619-090503.png");
    CHECK_EQ(r, 28);

    // No padding needed (all fields full width).
    r = ms_build_filename(buf, sizeof buf, 2026, 12, 31, 23, 59, 59);
    CHECK_STR(buf, "minishot-20261231-235959.png");
    CHECK_EQ(r, 28);

    // Zeros across the board.
    r = ms_build_filename(buf, sizeof buf, 0, 0, 0, 0, 0, 0);
    CHECK_STR(buf, "minishot-00000000-000000.png");
    CHECK_EQ(r, 28);

    // Buffer exactly one too small (28 chars + NUL needs 29).
    r = ms_build_filename(buf, 28, 2026, 6, 19, 9, 5, 3);
    CHECK_EQ(r, -1);

    // Buffer exactly large enough.
    r = ms_build_filename(buf, 29, 2026, 6, 19, 9, 5, 3);
    CHECK_EQ(r, 28);
    CHECK_STR(buf, "minishot-20260619-090503.png");

    TEST_REPORT();
}
