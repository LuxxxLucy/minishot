#include <string.h>
#include "test.h"
#include "pinmodel.h"

// --- ms_pin_create ---------------------------------------------------------
// Stores source pixel dims + scale, places the pin at the given origin, and
// sets initial w,h to px->points (ms_px_to_points: px / scale).

static void test_create_retina(void)
{
    // 200x100 px capture at scale 2.0 -> 100x50 points, placed at (10, 20).
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 10.0f, 20.0f);
    CHECK_NEAR(p.x, 10.0f, 1e-4);
    CHECK_NEAR(p.y, 20.0f, 1e-4);
    CHECK_NEAR(p.src_w, 200.0f, 1e-4);
    CHECK_NEAR(p.src_h, 100.0f, 1e-4);
    CHECK_NEAR(p.scale, 2.0f, 1e-4);
    CHECK_NEAR(p.w, 100.0f, 1e-4);
    CHECK_NEAR(p.h, 50.0f, 1e-4);
}

static void test_create_scale_one(void)
{
    // scale 1.0 -> points == pixels.
    ms_pin p = ms_pin_create(640.0f, 480.0f, 1.0f, 0.0f, 0.0f);
    CHECK_NEAR(p.x, 0.0f, 1e-4);
    CHECK_NEAR(p.y, 0.0f, 1e-4);
    CHECK_NEAR(p.w, 640.0f, 1e-4);
    CHECK_NEAR(p.h, 480.0f, 1e-4);
    CHECK_NEAR(p.src_w, 640.0f, 1e-4);
    CHECK_NEAR(p.src_h, 480.0f, 1e-4);
    CHECK_NEAR(p.scale, 1.0f, 1e-4);
}

static void test_create_negative_origin(void)
{
    ms_pin p = ms_pin_create(300.0f, 300.0f, 3.0f, -50.0f, -25.0f);
    CHECK_NEAR(p.x, -50.0f, 1e-4);
    CHECK_NEAR(p.y, -25.0f, 1e-4);
    CHECK_NEAR(p.w, 100.0f, 1e-4);
    CHECK_NEAR(p.h, 100.0f, 1e-4);
}

// --- ms_pin_resize: aspect-lock -------------------------------------------
// Aspect-locked via ms_aspect_contain(src_w, src_h, target_w, target_h).
// src_w/src_h are pixels; aspect ratio is what matters.

static void test_resize_aspect_landscape(void)
{
    // 2:1 source. Target 200x200 contain -> 200x100.
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 200.0f, 200.0f);
    CHECK_NEAR(s.w, 200.0f, 1e-4);
    CHECK_NEAR(s.h, 100.0f, 1e-4);
    CHECK_NEAR(p.w, 200.0f, 1e-4);
    CHECK_NEAR(p.h, 100.0f, 1e-4);
}

static void test_resize_aspect_portrait(void)
{
    // 1:2 source. Target 200x200 contain -> 100x200.
    ms_pin p = ms_pin_create(100.0f, 200.0f, 2.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 200.0f, 200.0f);
    CHECK_NEAR(s.w, 100.0f, 1e-4);
    CHECK_NEAR(s.h, 200.0f, 1e-4);
    CHECK_NEAR(p.w, 100.0f, 1e-4);
    CHECK_NEAR(p.h, 200.0f, 1e-4);
}

static void test_resize_height_constrained(void)
{
    // 2:1 source, target box 400x100 -> width-bound by height: 200x100.
    ms_pin p = ms_pin_create(400.0f, 200.0f, 2.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 400.0f, 100.0f);
    CHECK_NEAR(s.w, 200.0f, 1e-4);
    CHECK_NEAR(s.h, 100.0f, 1e-4);
}

static void test_resize_preserves_aspect_ratio(void)
{
    // 16:9 source resized to an arbitrary box keeps the ratio.
    ms_pin p = ms_pin_create(1600.0f, 900.0f, 1.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 800.0f, 800.0f);
    CHECK_NEAR(s.w / s.h, 1600.0f / 900.0f, 1e-4);
    CHECK_NEAR(s.w, 800.0f, 1e-4);
    CHECK_NEAR(s.h, 450.0f, 1e-4);
}

// --- ms_pin_resize: min-dimension clamp ------------------------------------
// Result clamped so the MIN dimension is >= 32, preserving aspect ratio
// (i.e. scale the contained size up until the smaller side reaches 32).

static void test_resize_clamp_square_min(void)
{
    // 1:1 source, tiny target -> clamp both to 32.
    ms_pin p = ms_pin_create(100.0f, 100.0f, 1.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 10.0f, 10.0f);
    CHECK_NEAR(s.w, 32.0f, 1e-4);
    CHECK_NEAR(s.h, 32.0f, 1e-4);
    CHECK_NEAR(p.w, 32.0f, 1e-4);
    CHECK_NEAR(p.h, 32.0f, 1e-4);
}

static void test_resize_clamp_landscape_keeps_aspect(void)
{
    // 2:1 source, tiny target 10x10 -> contain gives 10x5; min dim (5) < 32,
    // scale up by 32/5 so h=32, w=64. Aspect preserved.
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 10.0f, 10.0f);
    CHECK_NEAR(s.h, 32.0f, 1e-4);
    CHECK_NEAR(s.w, 64.0f, 1e-4);
    CHECK_NEAR(s.w / s.h, 2.0f, 1e-4);
}

static void test_resize_clamp_portrait_keeps_aspect(void)
{
    // 1:2 source, tiny target -> width is the smaller side, clamp w to 32.
    ms_pin p = ms_pin_create(100.0f, 200.0f, 2.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 8.0f, 8.0f);
    CHECK_NEAR(s.w, 32.0f, 1e-4);
    CHECK_NEAR(s.h, 64.0f, 1e-4);
    CHECK_NEAR(s.h / s.w, 2.0f, 1e-4);
}

static void test_resize_at_min_boundary_no_clamp(void)
{
    // 1:1 source contained exactly to 32x32 -> min == 32, no scaling.
    ms_pin p = ms_pin_create(64.0f, 64.0f, 1.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 32.0f, 32.0f);
    CHECK_NEAR(s.w, 32.0f, 1e-4);
    CHECK_NEAR(s.h, 32.0f, 1e-4);
}

static void test_resize_above_min_unchanged(void)
{
    // Comfortably above the floor: no clamp applied.
    ms_pin p = ms_pin_create(100.0f, 100.0f, 1.0f, 0.0f, 0.0f);
    ms_size s = ms_pin_resize(&p, 80.0f, 80.0f);
    CHECK_NEAR(s.w, 80.0f, 1e-4);
    CHECK_NEAR(s.h, 80.0f, 1e-4);
}

// --- ms_pin_move -----------------------------------------------------------

static void test_move_basic(void)
{
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 10.0f, 20.0f);
    ms_pin_move(&p, 5.0f, 7.0f);
    CHECK_NEAR(p.x, 15.0f, 1e-4);
    CHECK_NEAR(p.y, 27.0f, 1e-4);
    // Size must not change on a move.
    CHECK_NEAR(p.w, 100.0f, 1e-4);
    CHECK_NEAR(p.h, 50.0f, 1e-4);
}

static void test_move_negative(void)
{
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 10.0f, 20.0f);
    ms_pin_move(&p, -30.0f, -50.0f);
    CHECK_NEAR(p.x, -20.0f, 1e-4);
    CHECK_NEAR(p.y, -30.0f, 1e-4);
}

static void test_move_accumulates(void)
{
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 0.0f, 0.0f);
    ms_pin_move(&p, 1.0f, 2.0f);
    ms_pin_move(&p, 3.0f, 4.0f);
    ms_pin_move(&p, -2.0f, -1.0f);
    CHECK_NEAR(p.x, 2.0f, 1e-4);
    CHECK_NEAR(p.y, 5.0f, 1e-4);
}

static void test_move_zero(void)
{
    ms_pin p = ms_pin_create(200.0f, 100.0f, 2.0f, 10.0f, 20.0f);
    ms_pin_move(&p, 0.0f, 0.0f);
    CHECK_NEAR(p.x, 10.0f, 1e-4);
    CHECK_NEAR(p.y, 20.0f, 1e-4);
}

int main(void)
{
    test_create_retina();
    test_create_scale_one();
    test_create_negative_origin();

    test_resize_aspect_landscape();
    test_resize_aspect_portrait();
    test_resize_height_constrained();
    test_resize_preserves_aspect_ratio();

    test_resize_clamp_square_min();
    test_resize_clamp_landscape_keeps_aspect();
    test_resize_clamp_portrait_keeps_aspect();
    test_resize_at_min_boundary_no_clamp();
    test_resize_above_min_unchanged();

    test_move_basic();
    test_move_negative();
    test_move_accumulates();
    test_move_zero();

    TEST_REPORT();
}
