#include <string.h>
#include "test.h"
#include "capture.h"

int main(void)
{
    // Exact argv contents + argc + NULL termination.
    {
        char *argv[8];
        char buf[64];
        int argc = ms_capture_build_argv(argv, 8, buf, sizeof(buf), 10, 20, 300,
                                         200, "/tmp/a.png");
        CHECK_EQ(argc, 5);
        CHECK_STR(argv[0], "/usr/sbin/screencapture");
        CHECK_STR(argv[1], "-x");
        CHECK_STR(argv[2], "-R");
        CHECK_STR(argv[3], "10,20,300,200");
        CHECK_STR(argv[4], "/tmp/a.png");
        CHECK(argv[5] == NULL);
        // path entry points at the passed-in pointer directly (no copy).
        const char *p = "/tmp/a.png";
        char *argv2[8];
        char buf2[64];
        ms_capture_build_argv(argv2, 8, buf2, sizeof(buf2), 10, 20, 300, 200,
                              p);
        CHECK(argv2[4] == p);
    }

    // Negative coordinates format correctly.
    {
        char *argv[8];
        char buf[64];
        int argc = ms_capture_build_argv(argv, 8, buf, sizeof(buf), -5, -10, 1,
                                         2, "/x");
        CHECK_EQ(argc, 5);
        CHECK_STR(argv[3], "-5,-10,1,2");
    }

    // Overflow: max too small to hold argc+1 (need 6, give 5) -> -1.
    {
        char *argv[8];
        char buf[64];
        int argc = ms_capture_build_argv(argv, 5, buf, sizeof(buf), 10, 20, 300,
                                         200, "/tmp/a.png");
        CHECK_EQ(argc, -1);
    }

    // Boundary: max exactly argc+1 (6) -> succeeds.
    {
        char *argv[8];
        char buf[64];
        int argc = ms_capture_build_argv(argv, 6, buf, sizeof(buf), 10, 20, 300,
                                         200, "/tmp/a.png");
        CHECK_EQ(argc, 5);
    }

    // Overflow: bufn too small for rect string -> -1.
    {
        char *argv[8];
        char buf[8];  // "10,20,300,200" needs 14 bytes incl NUL
        int argc = ms_capture_build_argv(argv, 8, buf, sizeof(buf), 10, 20, 300,
                                         200, "/tmp/a.png");
        CHECK_EQ(argc, -1);
    }

    // NULL path -> -1 (would otherwise save to a default location).
    {
        char *argv[8];
        char buf[64];
        int argc = ms_capture_build_argv(argv, 8, buf, sizeof(buf), 10, 20, 300,
                                         200, NULL);
        CHECK_EQ(argc, -1);
    }

    TEST_REPORT();
}
