#include <string.h>
#include "test.h"
#include "toolbar.h"

// ms_toolbar_hit: a horizontal row of n square buttons of side 'btn' starting
// at (ox,oy). Button i occupies x in [ox+i*btn, ox+(i+1)*btn), y in
// [oy, oy+btn). Returns 0..n-1 the point hits, or MINISHOT_BTN_NONE (-1).

int main(void)
{
    const float ox = 100.0f, oy = 200.0f, btn = 40.0f;
    const int n = 5;

    // --- enum contract ---
    CHECK_EQ(MINISHOT_BTN_NONE, -1);
    CHECK_EQ(MINISHOT_BTN_PIN, 0);
    CHECK_EQ(MINISHOT_BTN_COPY, 1);
    CHECK_EQ(MINISHOT_BTN_SAVE, 2);
    CHECK_EQ(MINISHOT_BTN_DELETE, 3);
    CHECK_EQ(MINISHOT_BTN_CANCEL, 4);

    // --- center of each button hits that button ---
    for (int i = 0; i < n; i++) {
        float cx = ox + i * btn + btn * 0.5f;
        float cy = oy + btn * 0.5f;
        CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, cx, cy), i);
    }

    // --- centers map to the named buttons ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 0 * btn + 20, oy + 20),
             MINISHOT_BTN_PIN);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 1 * btn + 20, oy + 20),
             MINISHOT_BTN_COPY);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 2 * btn + 20, oy + 20),
             MINISHOT_BTN_SAVE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 3 * btn + 20, oy + 20),
             MINISHOT_BTN_DELETE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 4 * btn + 20, oy + 20),
             MINISHOT_BTN_CANCEL);

    // --- left edge of a button is inside (half-open: x >= ox+i*btn) ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox, oy + 5),
             0);  // first button left edge
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + btn, oy + 5),
             1);  // boundary belongs to right button
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 2 * btn, oy + 5), 2);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 3 * btn, oy + 5), 3);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 4 * btn, oy + 5), 4);

    // --- top edge inside ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 5, oy), 0);

    // --- just inside the right and bottom extents of the whole row ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + n * btn - 0.001f,
                            oy + btn - 0.001f),
             n - 1);

    // --- gaps/outside -> NONE ---
    // left of row
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox - 0.001f, oy + 20),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox - 100, oy + 20),
             MINISHOT_BTN_NONE);
    // right of row (boundary at ox + n*btn is exclusive)
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + n * btn, oy + 20),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + n * btn + 0.001f, oy + 20),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 1000, oy + 20),
             MINISHOT_BTN_NONE);
    // above the row
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 20, oy - 0.001f),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 20, oy - 100),
             MINISHOT_BTN_NONE);
    // below the row (boundary at oy + btn is exclusive)
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 20, oy + btn),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 20, oy + btn + 0.001f),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 20, oy + 1000),
             MINISHOT_BTN_NONE);
    // diagonally outside (both axes out)
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox - 50, oy - 50),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, n, ox + 1000, oy + 1000),
             MINISHOT_BTN_NONE);

    // --- origin at (0,0): regression against assuming nonzero offset ---
    CHECK_EQ(ms_toolbar_hit(0, 0, btn, n, 20, 20), 0);
    CHECK_EQ(ms_toolbar_hit(0, 0, btn, n, btn + 20, 20), 1);
    CHECK_EQ(ms_toolbar_hit(0, 0, btn, n, -1, 20), MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(0, 0, btn, n, 20, -1), MINISHOT_BTN_NONE);

    // --- negative origin (toolbar above selection) ---
    CHECK_EQ(ms_toolbar_hit(-50, -30, btn, n, -50 + 20, -30 + 20), 0);
    CHECK_EQ(ms_toolbar_hit(-50, -30, btn, n, -50 - 1, -30 + 20),
             MINISHOT_BTN_NONE);

    // --- n = 1: single button row, only that one column is hot ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, 1, ox + 20, oy + 20), 0);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, 1, ox + btn + 1, oy + 20),
             MINISHOT_BTN_NONE);

    // --- n = 0: nothing is hittable ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, 0, ox + 20, oy + 20),
             MINISHOT_BTN_NONE);

    // --- a point valid for n=5 columns is rejected when n is smaller ---
    // column index 4 (CANCEL) lies outside a 3-button row.
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, 3, ox + 4 * btn + 20, oy + 20),
             MINISHOT_BTN_NONE);
    CHECK_EQ(ms_toolbar_hit(ox, oy, btn, 3, ox + 2 * btn + 20, oy + 20), 2);

    // --- different button size scales the columns ---
    CHECK_EQ(ms_toolbar_hit(ox, oy, 10.0f, n, ox + 25, oy + 5),
             2);  // 25/10 -> col 2
    CHECK_EQ(ms_toolbar_hit(ox, oy, 10.0f, n, ox + 5, oy + 11),
             MINISHOT_BTN_NONE);  // y past btn

    TEST_REPORT();
}
