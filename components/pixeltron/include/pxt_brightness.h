// Knob-driven brightness.
//
// Turning the knob changes brightness locally right away. The new level is
// reported to the server once the knob has been still for
// PXT_BRIGHTNESS_REPORT_DELAY_MS, and server brightness messages are ignored
// until then; otherwise the server's next "brightness" message would undo
// the turn.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PXT_BRIGHTNESS_STEP 5
#define PXT_BRIGHTNESS_REPORT_DELAY_MS 500
#define PXT_BRIGHTNESS_OVERLAY_MS 1500

typedef struct {
  int level;  // 0-100
  bool unreported;
  bool turned;  // at least one turn since init, for the overlay
  uint32_t changed_at;
} pxt_brightness_t;

void pxt_brightness_init(pxt_brightness_t* b, int level);

// Positive detents brighten, negative dim. Returns the new level.
int pxt_brightness_turn(pxt_brightness_t* b, int detents, uint32_t now_ms);

// True once per settled turn; *level gets the value to send.
bool pxt_brightness_take_report(pxt_brightness_t* b, uint32_t now_ms,
                                int* level);

// Apply a brightness from the server unless a local change is pending.
// Returns true if it was applied.
bool pxt_brightness_from_server(pxt_brightness_t* b, int level);

bool pxt_brightness_overlay_visible(const pxt_brightness_t* b,
                                    uint32_t now_ms);

#ifdef __cplusplus
}
#endif
