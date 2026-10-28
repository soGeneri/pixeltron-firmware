#include "pxt_brightness.h"

static int clamp_level(int level) {
  if (level < 0) return 0;
  if (level > 100) return 100;
  return level;
}

void pxt_brightness_init(pxt_brightness_t* b, int level) {
  b->level = clamp_level(level);
  b->unreported = false;
  b->turned = false;
  b->changed_at = 0;
}

int pxt_brightness_turn(pxt_brightness_t* b, int detents, uint32_t now_ms) {
  if (detents == 0) return b->level;
  b->level = clamp_level(b->level + detents * PXT_BRIGHTNESS_STEP);
  b->unreported = true;
  b->turned = true;
  b->changed_at = now_ms;
  return b->level;
}

bool pxt_brightness_take_report(pxt_brightness_t* b, uint32_t now_ms,
                                int* level) {
  if (!b->unreported) return false;
  if (now_ms - b->changed_at < PXT_BRIGHTNESS_REPORT_DELAY_MS) return false;
  b->unreported = false;
  *level = b->level;
  return true;
}

bool pxt_brightness_from_server(pxt_brightness_t* b, int level) {
  if (b->unreported) return false;
  b->level = clamp_level(level);
  return true;
}

bool pxt_brightness_overlay_visible(const pxt_brightness_t* b,
                                    uint32_t now_ms) {
  return b->turned && now_ms - b->changed_at < PXT_BRIGHTNESS_OVERLAY_MS;
}
