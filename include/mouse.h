#ifndef MOUSE_SYSTEM
#define MOUSE_SYSTEM

#include <Arduino.h>

enum class mouseEvent_t : byte {
    LMB_PRESS, LMB_RELEASE,
    RMB_PRESS, RMB_RELEASE,
    MMB_PRESS, MMB_RELEASE,
    SCROLL_UP, SCROLL_DOWN,
    SCROLL_LEFT, SCROLL_RIGHT,
    FWD_PRESS, FWD_RELEASE,
    BACK_PRESS, BACK_RELEASE
};

#endif