#include "appstate.h"

ms_app_state ms_app_next(ms_app_state cur, ms_app_event ev)
{
    switch (cur) {
        case MINISHOT_IDLE:
            if (ev == MINISHOT_EV_HOTKEY) {
                return MINISHOT_SELECTING;
            }
            break;
        case MINISHOT_SELECTING:
            if (ev == MINISHOT_EV_SELECT_DONE) {
                return MINISHOT_ACTION_MENU;
            }
            if (ev == MINISHOT_EV_SELECT_CANCEL) {
                return MINISHOT_IDLE;
            }
            break;
        case MINISHOT_ACTION_MENU:
            if (ev == MINISHOT_EV_ACTION_CHOSEN) {
                return MINISHOT_IDLE;
            }
            if (ev == MINISHOT_EV_SELECT_CANCEL) {
                return MINISHOT_IDLE;
            }
            break;
    }
    return cur;
}
