#include <string.h>
#include "test.h"
#include "toast.h"

int main(void)
{
    // ---- ms_toast_show: basic field population ----
    {
        ms_toast t;
        memset(&t, 0xAB,
               sizeof t);  // poison to prove show() writes every field
        ms_toast_show(&t, "hello", 10.0, 3.0);
        CHECK_STR(t.msg, "hello");
        CHECK_EQ(t.active, 1);
        CHECK_NEAR(t.until, 13.0, 1e-4);
    }

    // ---- until = now + dur arithmetic ----
    {
        ms_toast t;
        ms_toast_show(&t, "x", 100.5, 0.25);
        CHECK_NEAR(t.until, 100.75, 1e-4);
    }
    {
        ms_toast t;
        ms_toast_show(&t, "x", -5.0, 2.0);  // negative now
        CHECK_NEAR(t.until, -3.0, 1e-4);
    }
    {
        ms_toast t;
        ms_toast_show(&t, "x", 7.0, 0.0);  // zero duration
        CHECK_NEAR(t.until, 7.0, 1e-4);
    }

    // ---- empty message ----
    {
        ms_toast t;
        ms_toast_show(&t, "", 1.0, 1.0);
        CHECK_STR(t.msg, "");
        CHECK_EQ(t.active, 1);
    }

    // ---- bounded copy: long message must not overflow msg[256] ----
    {
        ms_toast t;
        char big[600];
        memset(big, 'Z', sizeof big);
        big[sizeof big - 1] = '\0';
        ms_toast_show(&t, big, 0.0, 1.0);
        // must be NUL-terminated within the 256-byte buffer
        CHECK_EQ(t.msg[255], 0);
        CHECK(strlen(t.msg) <= 255);
        // and the prefix that fits must be the source content
        CHECK_EQ(t.msg[0], 'Z');
        CHECK_EQ(t.msg[254], 'Z');
    }

    // ---- exact-fit message (255 chars + NUL) ----
    {
        ms_toast t;
        char fit[256];
        memset(fit, 'Q', 255);
        fit[255] = '\0';
        ms_toast_show(&t, fit, 0.0, 1.0);
        CHECK_EQ((int)strlen(t.msg), 255);
        CHECK_EQ(t.msg[255], 0);
        CHECK_STR(t.msg, fit);
    }

    // ---- show overwrites a prior message ----
    {
        ms_toast t;
        ms_toast_show(&t, "first", 0.0, 5.0);
        ms_toast_show(&t, "second", 0.0, 5.0);
        CHECK_STR(t.msg, "second");
    }

    // ---- ms_toast_visible: the now<until vs now>=until boundary ----
    {
        ms_toast t;
        ms_toast_show(&t, "m", 10.0, 5.0);           // until = 15.0
        CHECK_EQ(ms_toast_visible(&t, 10.0), 1);     // now < until
        CHECK_EQ(ms_toast_visible(&t, 14.9999), 1);  // just before until
        CHECK_EQ(ms_toast_visible(&t, 14.0), 1);     // strictly inside
    }
    {
        ms_toast t;
        ms_toast_show(&t, "m", 10.0, 5.0);        // until = 15.0
        CHECK_EQ(ms_toast_visible(&t, 15.0), 0);  // now == until -> NOT visible
        CHECK_EQ(ms_toast_visible(&t, 15.0001), 0);  // just after until
        CHECK_EQ(ms_toast_visible(&t, 100.0), 0);    // long after
    }

    // ---- zero-duration toast is never visible (now >= until immediately) ----
    {
        ms_toast t;
        ms_toast_show(&t, "m", 7.0, 0.0);  // until = 7.0
        CHECK_EQ(ms_toast_visible(&t, 7.0), 0);
    }

    // ---- inactive toast is never visible regardless of time ----
    {
        ms_toast t;
        memset(&t, 0, sizeof t);  // active = 0
        t.until = 1e9;
        CHECK_EQ(ms_toast_visible(&t, 0.0), 0);
    }

    TEST_REPORT();
}
