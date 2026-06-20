#include <string.h>
#include "test.h"
#include "geometry.h"

static void test_rect_from_drag(void)
{
    // Forward drag: anchor top-left, current bottom-right.
    struct ms_rect a = ms_rect_from_drag(10, 10, 50, 40);
    CHECK_NEAR(a.x, 10, 1e-4);
    CHECK_NEAR(a.y, 10, 1e-4);
    CHECK_NEAR(a.w, 40, 1e-4);
    CHECK_NEAR(a.h, 30, 1e-4);

    // Reversed drag: anchor bottom-right, current top-left -> same rect.
    struct ms_rect b = ms_rect_from_drag(50, 40, 10, 10);
    CHECK_NEAR(b.x, 10, 1e-4);
    CHECK_NEAR(b.y, 10, 1e-4);
    CHECK_NEAR(b.w, 40, 1e-4);
    CHECK_NEAR(b.h, 30, 1e-4);

    // Mixed direction: anchor top-right, current bottom-left.
    struct ms_rect c = ms_rect_from_drag(50, 10, 10, 40);
    CHECK_NEAR(c.x, 10, 1e-4);
    CHECK_NEAR(c.y, 10, 1e-4);
    CHECK_NEAR(c.w, 40, 1e-4);
    CHECK_NEAR(c.h, 30, 1e-4);

    // Degenerate drag (no movement) -> zero size at the point.
    struct ms_rect d = ms_rect_from_drag(7, 8, 7, 8);
    CHECK_NEAR(d.x, 7, 1e-4);
    CHECK_NEAR(d.y, 8, 1e-4);
    CHECK_NEAR(d.w, 0, 1e-4);
    CHECK_NEAR(d.h, 0, 1e-4);

    // Negative coordinates.
    struct ms_rect e = ms_rect_from_drag(-5, -5, -20, 10);
    CHECK_NEAR(e.x, -20, 1e-4);
    CHECK_NEAR(e.y, -5, 1e-4);
    CHECK_NEAR(e.w, 15, 1e-4);
    CHECK_NEAR(e.h, 15, 1e-4);
}

static void test_rect_to_screen(void)
{
    struct ms_rect r = { 10, 20, 30, 40 };
    struct ms_rect s = ms_rect_to_screen(r, 100, 200);
    CHECK_NEAR(s.x, 110, 1e-4);
    CHECK_NEAR(s.y, 220, 1e-4);
    CHECK_NEAR(s.w, 30, 1e-4);
    CHECK_NEAR(s.h, 40, 1e-4);

    // Zero origin is a no-op.
    struct ms_rect z = ms_rect_to_screen(r, 0, 0);
    CHECK_NEAR(z.x, 10, 1e-4);
    CHECK_NEAR(z.y, 20, 1e-4);
    CHECK_NEAR(z.w, 30, 1e-4);
    CHECK_NEAR(z.h, 40, 1e-4);

    // Negative origin (window off-screen left/up).
    struct ms_rect n = ms_rect_to_screen(r, -15, -25);
    CHECK_NEAR(n.x, -5, 1e-4);
    CHECK_NEAR(n.y, -5, 1e-4);
    CHECK_NEAR(n.w, 30, 1e-4);
    CHECK_NEAR(n.h, 40, 1e-4);
}

static void test_aspect_contain(void)
{
    // Wide source, square box -> width-limited.
    struct ms_size a = ms_aspect_contain(200, 100, 50, 50);
    CHECK_NEAR(a.w, 50, 1e-4);
    CHECK_NEAR(a.h, 25, 1e-4);

    // Same wide source, larger square box -> still width-limited, scaled up.
    struct ms_size b = ms_aspect_contain(200, 100, 300, 300);
    CHECK_NEAR(b.w, 300, 1e-4);
    CHECK_NEAR(b.h, 150, 1e-4);

    // Tall source, square box -> height-limited.
    struct ms_size c = ms_aspect_contain(100, 200, 50, 50);
    CHECK_NEAR(c.w, 25, 1e-4);
    CHECK_NEAR(c.h, 50, 1e-4);

    // Exact fit.
    struct ms_size d = ms_aspect_contain(100, 100, 100, 100);
    CHECK_NEAR(d.w, 100, 1e-4);
    CHECK_NEAR(d.h, 100, 1e-4);

    // Non-square box where height is the binding constraint.
    struct ms_size e = ms_aspect_contain(100, 100, 200, 80);
    CHECK_NEAR(e.w, 80, 1e-4);
    CHECK_NEAR(e.h, 80, 1e-4);

    // Guards: non-positive source returns {0,0}.
    struct ms_size g1 = ms_aspect_contain(0, 100, 50, 50);
    CHECK_NEAR(g1.w, 0, 1e-4);
    CHECK_NEAR(g1.h, 0, 1e-4);
    struct ms_size g2 = ms_aspect_contain(100, 0, 50, 50);
    CHECK_NEAR(g2.w, 0, 1e-4);
    CHECK_NEAR(g2.h, 0, 1e-4);
    struct ms_size g3 = ms_aspect_contain(-1, 100, 50, 50);
    CHECK_NEAR(g3.w, 0, 1e-4);
    CHECK_NEAR(g3.h, 0, 1e-4);

    // Guards: non-positive target box returns {0,0}.
    struct ms_size g4 = ms_aspect_contain(200, 100, 0, 50);
    CHECK_NEAR(g4.w, 0, 1e-4);
    CHECK_NEAR(g4.h, 0, 1e-4);
    struct ms_size g5 = ms_aspect_contain(200, 100, 50, -1);
    CHECK_NEAR(g5.w, 0, 1e-4);
    CHECK_NEAR(g5.h, 0, 1e-4);
}

static void test_px_to_points(void)
{
    struct ms_size a = ms_px_to_points(200, 100, 2.0);
    CHECK_NEAR(a.w, 100, 1e-4);
    CHECK_NEAR(a.h, 50, 1e-4);

    // Scale 1.0 is identity.
    struct ms_size b = ms_px_to_points(200, 100, 1.0);
    CHECK_NEAR(b.w, 200, 1e-4);
    CHECK_NEAR(b.h, 100, 1e-4);

    // Fractional scale.
    struct ms_size c = ms_px_to_points(300, 150, 1.5);
    CHECK_NEAR(c.w, 200, 1e-4);
    CHECK_NEAR(c.h, 100, 1e-4);

    // scale <= 0 is treated as 1.0.
    struct ms_size d = ms_px_to_points(200, 100, 0.0);
    CHECK_NEAR(d.w, 200, 1e-4);
    CHECK_NEAR(d.h, 100, 1e-4);
    struct ms_size e = ms_px_to_points(200, 100, -2.0);
    CHECK_NEAR(e.w, 200, 1e-4);
    CHECK_NEAR(e.h, 100, 1e-4);
}

// Contain, then scale up (aspect preserved) until the smaller side >= min_dim.
static void test_aspect_contain_min(void)
{
    // Comfortably above the floor: behaves like ms_aspect_contain.
    struct ms_size a = ms_aspect_contain_min(100, 100, 80, 80, 32);
    CHECK_NEAR(a.w, 80, 1e-4);
    CHECK_NEAR(a.h, 80, 1e-4);

    // 1:1 source, tiny box -> both clamped to 32.
    struct ms_size b = ms_aspect_contain_min(100, 100, 10, 10, 32);
    CHECK_NEAR(b.w, 32, 1e-4);
    CHECK_NEAR(b.h, 32, 1e-4);

    // 2:1 source, tiny box -> contain gives 10x5; min (5) < 32 -> 64x32.
    struct ms_size c = ms_aspect_contain_min(200, 100, 10, 10, 32);
    CHECK_NEAR(c.w, 64, 1e-4);
    CHECK_NEAR(c.h, 32, 1e-4);
    CHECK_NEAR(c.w / c.h, 2.0f, 1e-4);

    // 1:2 source -> width is the smaller side, clamp w to 32 -> 32x64.
    struct ms_size d = ms_aspect_contain_min(100, 200, 8, 8, 32);
    CHECK_NEAR(d.w, 32, 1e-4);
    CHECK_NEAR(d.h, 64, 1e-4);

    // Exactly at the floor: no scaling.
    struct ms_size e = ms_aspect_contain_min(64, 64, 32, 32, 32);
    CHECK_NEAR(e.w, 32, 1e-4);
    CHECK_NEAR(e.h, 32, 1e-4);
}

int main(void)
{
    test_rect_from_drag();
    test_rect_to_screen();
    test_aspect_contain();
    test_aspect_contain_min();
    test_px_to_points();
    TEST_REPORT();
}
