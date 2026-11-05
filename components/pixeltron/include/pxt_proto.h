// Device -> server websocket messages for the Pixeltron knob and updates.
//
// The server (pixeltron-server internal/server/websockets.go) turns these
// into the app's live events:
//   {"brightness":40}        dial turn (brightness the knob set)
//   {"button":"next"}        press           -> dial press
//   {"button":"previous"}    double press    -> dial press (back one app)
//   {"button":"back"}        hold 2-3 s, let go -> dial release
//   {"hold":3} ... {"hold":9} reset countdown, seconds held, once a second
//   {"hold":0}               let go before the reset -> dial release
//   {"hold":10}              reset happens now
//   {"ota":{"progress":0.4}} / {"ota":{"done":true}} / {"ota":{"error":"..."}}
//
// Plain C with no ESP-IDF dependencies, so the host tests cover it.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "pxt_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int last_hold;  // seconds held last reported, 0 when none
} pxt_proto_t;

void pxt_proto_init(pxt_proto_t* p);

// Each returns the message length written to out, or 0 if there's nothing to
// send. out needs at least 48 bytes.

// A finished gesture from pxt_gesture_update().
int pxt_proto_gesture(pxt_proto_t* p, pxt_gesture_t g, char* out, size_t n);

// Call often with pxt_gesture_reset_countdown(); reports each new whole
// second held during the countdown.
int pxt_proto_countdown(pxt_proto_t* p, int seconds_left, char* out, size_t n);

int pxt_proto_brightness(int level, char* out, size_t n);

// percent < 0 omits progress; error may be NULL.
int pxt_proto_ota(int percent, bool done, const char* error, char* out,
                  size_t n);

#ifdef __cplusplus
}
#endif
