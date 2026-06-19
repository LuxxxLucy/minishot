#ifndef MINISHOT_APPSTATE_H
#define MINISHOT_APPSTATE_H

typedef enum {
    MINISHOT_IDLE,
    MINISHOT_SELECTING,
    MINISHOT_ACTION_MENU,
} ms_app_state;

typedef enum {
    MINISHOT_EV_HOTKEY,
    MINISHOT_EV_SELECT_DONE,
    MINISHOT_EV_SELECT_CANCEL,
    MINISHOT_EV_ACTION_CHOSEN,
} ms_app_event;

// Capture state machine. Transitions:
//   IDLE        + HOTKEY        -> SELECTING
//   SELECTING   + SELECT_DONE   -> ACTION_MENU
//   SELECTING   + SELECT_CANCEL -> IDLE
//   ACTION_MENU + ACTION_CHOSEN -> IDLE
//   ACTION_MENU + SELECT_CANCEL -> IDLE
//   any other (state,event)     -> unchanged
// PURE.
ms_app_state ms_app_next(ms_app_state cur, ms_app_event ev);

#endif
