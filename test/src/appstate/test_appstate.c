#include <string.h>
#include "test.h"
#include "appstate.h"

// Exhaustive test of the capture state machine ms_app_next(cur, ev).
//
// Defined transitions:
//   IDLE        + HOTKEY        -> SELECTING
//   SELECTING   + SELECT_DONE   -> ACTION_MENU
//   SELECTING   + SELECT_CANCEL -> IDLE
//   ACTION_MENU + ACTION_CHOSEN -> IDLE
//   ACTION_MENU + SELECT_CANCEL -> IDLE
//   any other (state,event)     -> unchanged

int main(void)
{
    // ---- The five explicit transitions ----
    CHECK_EQ(ms_app_next(MINISHOT_IDLE, MINISHOT_EV_HOTKEY),
             MINISHOT_SELECTING);
    CHECK_EQ(ms_app_next(MINISHOT_SELECTING, MINISHOT_EV_SELECT_DONE),
             MINISHOT_ACTION_MENU);
    CHECK_EQ(ms_app_next(MINISHOT_SELECTING, MINISHOT_EV_SELECT_CANCEL),
             MINISHOT_IDLE);
    CHECK_EQ(ms_app_next(MINISHOT_ACTION_MENU, MINISHOT_EV_ACTION_CHOSEN),
             MINISHOT_IDLE);
    CHECK_EQ(ms_app_next(MINISHOT_ACTION_MENU, MINISHOT_EV_SELECT_CANCEL),
             MINISHOT_IDLE);

    // ---- No-op fallthrough: every remaining (state,event) pair stays
    // unchanged ----

    // IDLE: only HOTKEY is meaningful; the rest are no-ops.
    CHECK_EQ(ms_app_next(MINISHOT_IDLE, MINISHOT_EV_SELECT_DONE),
             MINISHOT_IDLE);
    CHECK_EQ(ms_app_next(MINISHOT_IDLE, MINISHOT_EV_SELECT_CANCEL),
             MINISHOT_IDLE);
    CHECK_EQ(ms_app_next(MINISHOT_IDLE, MINISHOT_EV_ACTION_CHOSEN),
             MINISHOT_IDLE);

    // SELECTING: only SELECT_DONE / SELECT_CANCEL are meaningful.
    CHECK_EQ(ms_app_next(MINISHOT_SELECTING, MINISHOT_EV_HOTKEY),
             MINISHOT_SELECTING);
    CHECK_EQ(ms_app_next(MINISHOT_SELECTING, MINISHOT_EV_ACTION_CHOSEN),
             MINISHOT_SELECTING);

    // ACTION_MENU: only ACTION_CHOSEN / SELECT_CANCEL are meaningful.
    CHECK_EQ(ms_app_next(MINISHOT_ACTION_MENU, MINISHOT_EV_HOTKEY),
             MINISHOT_ACTION_MENU);
    CHECK_EQ(ms_app_next(MINISHOT_ACTION_MENU, MINISHOT_EV_SELECT_DONE),
             MINISHOT_ACTION_MENU);

    // ---- Determinism: same input twice yields same output ----
    CHECK_EQ(ms_app_next(MINISHOT_IDLE, MINISHOT_EV_HOTKEY),
             ms_app_next(MINISHOT_IDLE, MINISHOT_EV_HOTKEY));
    CHECK_EQ(ms_app_next(MINISHOT_ACTION_MENU, MINISHOT_EV_ACTION_CHOSEN),
             ms_app_next(MINISHOT_ACTION_MENU, MINISHOT_EV_ACTION_CHOSEN));

    // ---- Full happy-path round trip: IDLE -> SELECTING -> ACTION_MENU -> IDLE
    // ----
    {
        ms_app_state s = MINISHOT_IDLE;
        s = ms_app_next(s, MINISHOT_EV_HOTKEY);
        CHECK_EQ(s, MINISHOT_SELECTING);
        s = ms_app_next(s, MINISHOT_EV_SELECT_DONE);
        CHECK_EQ(s, MINISHOT_ACTION_MENU);
        s = ms_app_next(s, MINISHOT_EV_ACTION_CHOSEN);
        CHECK_EQ(s, MINISHOT_IDLE);
    }

    // ---- Cancel-from-selecting path: IDLE -> SELECTING -> IDLE ----
    {
        ms_app_state s = MINISHOT_IDLE;
        s = ms_app_next(s, MINISHOT_EV_HOTKEY);
        CHECK_EQ(s, MINISHOT_SELECTING);
        s = ms_app_next(s, MINISHOT_EV_SELECT_CANCEL);
        CHECK_EQ(s, MINISHOT_IDLE);
    }

    // ---- Cancel-from-menu path: IDLE -> SELECTING -> ACTION_MENU -> IDLE (via
    // cancel) ----
    {
        ms_app_state s = MINISHOT_IDLE;
        s = ms_app_next(s, MINISHOT_EV_HOTKEY);
        s = ms_app_next(s, MINISHOT_EV_SELECT_DONE);
        CHECK_EQ(s, MINISHOT_ACTION_MENU);
        s = ms_app_next(s, MINISHOT_EV_SELECT_CANCEL);
        CHECK_EQ(s, MINISHOT_IDLE);
    }

    // ---- Idle no-op churn: a stray hotkey while idle must not advance past
    // SELECTING,
    //      and stray non-hotkey events while idle keep us put. ----
    {
        ms_app_state s = MINISHOT_IDLE;
        s = ms_app_next(s, MINISHOT_EV_SELECT_DONE);  // no-op
        CHECK_EQ(s, MINISHOT_IDLE);
        s = ms_app_next(s, MINISHOT_EV_ACTION_CHOSEN);  // no-op
        CHECK_EQ(s, MINISHOT_IDLE);
        s = ms_app_next(s, MINISHOT_EV_HOTKEY);  // now advance
        CHECK_EQ(s, MINISHOT_SELECTING);
    }

    TEST_REPORT();
}
