#include <stdio.h>
#include <string.h>

#include "pxt_proto.h"

#define PXT_RESET_SECS (PXT_RESET_MS / 1000)

static int fit(int written, size_t n) {
  return (written > 0 && (size_t)written < n) ? written : 0;
}

void pxt_proto_init(pxt_proto_t* p) { p->last_hold = 0; }

int pxt_proto_gesture(pxt_proto_t* p, pxt_gesture_t g, char* out, size_t n) {
  switch (g) {
    case PXT_GESTURE_PRESS:
      return fit(snprintf(out, n, "{\"button\":\"next\"}"), n);
    case PXT_GESTURE_DOUBLE_PRESS:
      return fit(snprintf(out, n, "{\"button\":\"previous\"}"), n);
    case PXT_GESTURE_HOLD:
      return fit(snprintf(out, n, "{\"button\":\"back\"}"), n);
    case PXT_GESTURE_RESET:
      p->last_hold = 0;
      return fit(snprintf(out, n, "{\"hold\":%d}", PXT_RESET_SECS), n);
    case PXT_GESTURE_RESET_CANCELLED:
      p->last_hold = 0;
      return fit(snprintf(out, n, "{\"hold\":0}"), n);
    default:
      return 0;
  }
}

int pxt_proto_countdown(pxt_proto_t* p, int seconds_left, char* out, size_t n) {
  if (seconds_left < 0) return 0;
  int held = PXT_RESET_SECS - seconds_left;
  if (held <= 0 || held >= PXT_RESET_SECS || held == p->last_hold) return 0;
  p->last_hold = held;
  return fit(snprintf(out, n, "{\"hold\":%d}", held), n);
}

int pxt_proto_brightness(int level, char* out, size_t n) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  return fit(snprintf(out, n, "{\"brightness\":%d}", level), n);
}

int pxt_proto_ota(int percent, bool done, const char* error, char* out,
                  size_t n) {
  if (error && *error) {
    // Keep the message JSON-safe: drop quotes and backslashes.
    char clean[40];
    size_t j = 0;
    for (size_t i = 0; error[i] && j + 1 < sizeof(clean); i++) {
      if (error[i] != '"' && error[i] != '\\') clean[j++] = error[i];
    }
    clean[j] = '\0';
    return fit(snprintf(out, n, "{\"ota\":{\"error\":\"%s\"}}", clean), n);
  }
  if (done) return fit(snprintf(out, n, "{\"ota\":{\"done\":true}}"), n);
  if (percent < 0) return 0;
  if (percent > 100) percent = 100;
  return fit(snprintf(out, n, "{\"ota\":{\"progress\":%d.%02d}}", percent / 100,
                      percent % 100),
             n);
}
