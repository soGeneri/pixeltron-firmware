#include "pxt_gesture.h"

enum {
  ST_IDLE = 0,
  ST_DOWN,         // first press, still held
  ST_WAIT_SECOND,  // released a short press, waiting for a second one
  ST_SECOND_DOWN,  // second press of a double press, still held
  ST_RESET_FIRED,  // reset already reported, waiting for release
};

void pxt_gesture_init(pxt_gesture_state_t* st) {
  st->state = ST_IDLE;
  st->down_at = 0;
  st->up_at = 0;
}

pxt_gesture_t pxt_gesture_update(pxt_gesture_state_t* st, bool pressed,
                                 uint32_t now_ms) {
  switch (st->state) {
    case ST_IDLE:
      if (pressed) {
        st->state = ST_DOWN;
        st->down_at = now_ms;
      }
      return PXT_GESTURE_NONE;

    case ST_DOWN: {
      uint32_t held = now_ms - st->down_at;
      if (pressed) {
        if (held >= PXT_RESET_MS) {
          st->state = ST_RESET_FIRED;
          return PXT_GESTURE_RESET;
        }
        return PXT_GESTURE_NONE;
      }
      if (held < PXT_HOLD_MS) {
        st->state = ST_WAIT_SECOND;
        st->up_at = now_ms;
        return PXT_GESTURE_NONE;
      }
      st->state = ST_IDLE;
      // Letting go once the countdown is showing means "cancel", not "back".
      return held < PXT_COUNTDOWN_MS ? PXT_GESTURE_HOLD
                                     : PXT_GESTURE_RESET_CANCELLED;
    }

    case ST_WAIT_SECOND:
      if (pressed) {
        st->state = ST_SECOND_DOWN;
        st->down_at = now_ms;
        return PXT_GESTURE_NONE;
      }
      if (now_ms - st->up_at >= PXT_DOUBLE_PRESS_MS) {
        st->state = ST_IDLE;
        return PXT_GESTURE_PRESS;
      }
      return PXT_GESTURE_NONE;

    case ST_SECOND_DOWN:
      if (!pressed) {
        st->state = ST_IDLE;
        return PXT_GESTURE_DOUBLE_PRESS;
      }
      return PXT_GESTURE_NONE;

    case ST_RESET_FIRED:
    default:
      if (!pressed) st->state = ST_IDLE;
      return PXT_GESTURE_NONE;
  }
}

int pxt_gesture_reset_countdown(const pxt_gesture_state_t* st,
                                uint32_t now_ms) {
  if (st->state != ST_DOWN) return -1;
  uint32_t held = now_ms - st->down_at;
  if (held < PXT_COUNTDOWN_MS || held >= PXT_RESET_MS) return -1;
  return (int)((PXT_RESET_MS - held + 999) / 1000);
}

const char* pxt_gesture_name(pxt_gesture_t g) {
  switch (g) {
    case PXT_GESTURE_PRESS:
      return "press";
    case PXT_GESTURE_DOUBLE_PRESS:
      return "double_press";
    case PXT_GESTURE_HOLD:
      return "hold";
    case PXT_GESTURE_RESET:
      return "reset";
    case PXT_GESTURE_RESET_CANCELLED:
      return "reset_cancelled";
    default:
      return "none";
  }
}
