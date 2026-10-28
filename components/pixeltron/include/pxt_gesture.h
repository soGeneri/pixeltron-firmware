// Pixeltron knob button gestures.
//
// A pure state machine: feed it the debounced switch level and a millisecond
// clock, get back at most one gesture per call. No ESP-IDF dependencies, so
// the same code runs on the device, in host tests and in the simulator.
//
// Gesture map (docs/08-setup-experience.md):
//   press                  next app
//   double press           previous app
//   hold 2-3 s, release    back
//   hold 10 s              factory reset (countdown shown from 3 s)
//   release after 3 s      reset cancelled
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PXT_DOUBLE_PRESS_MS 300
#define PXT_HOLD_MS 2000
#define PXT_COUNTDOWN_MS 3000
#define PXT_RESET_MS 10000

typedef enum {
  PXT_GESTURE_NONE = 0,
  PXT_GESTURE_PRESS,
  PXT_GESTURE_DOUBLE_PRESS,
  PXT_GESTURE_HOLD,
  PXT_GESTURE_RESET,
  PXT_GESTURE_RESET_CANCELLED,
} pxt_gesture_t;

typedef struct {
  int state;
  uint32_t down_at;
  uint32_t up_at;
} pxt_gesture_state_t;

void pxt_gesture_init(pxt_gesture_state_t* st);

// Call at least every 50 ms with the current switch level (true = pressed).
pxt_gesture_t pxt_gesture_update(pxt_gesture_state_t* st, bool pressed,
                                 uint32_t now_ms);

// Whole seconds left before a reset while the knob is held past
// PXT_COUNTDOWN_MS, or -1 when no countdown should be shown.
int pxt_gesture_reset_countdown(const pxt_gesture_state_t* st,
                                uint32_t now_ms);

const char* pxt_gesture_name(pxt_gesture_t g);

#ifdef __cplusplus
}
#endif
