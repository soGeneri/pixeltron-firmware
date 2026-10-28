// Host unit tests for the Pixeltron component. Run with `make test`.
#include <stdio.h>
#include <string.h>

#include "pxt_brightness.h"
#include "pxt_codeword.h"
#include "pxt_gesture.h"
#include "pxt_screens.h"

static int failures = 0;

#define CHECK(cond)                                         \
  do {                                                      \
    if (!(cond)) {                                          \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                           \
    }                                                       \
  } while (0)

// Holds the switch at `pressed` from `*t` for `ms`, polling every 10 ms the
// way the firmware does, and returns the first gesture reported.
static pxt_gesture_t run(pxt_gesture_state_t* st, bool pressed, uint32_t* t,
                         uint32_t ms) {
  pxt_gesture_t first = PXT_GESTURE_NONE;
  for (uint32_t elapsed = 0; elapsed < ms; elapsed += 10, *t += 10) {
    pxt_gesture_t g = pxt_gesture_update(st, pressed, *t);
    if (g != PXT_GESTURE_NONE && first == PXT_GESTURE_NONE) first = g;
  }
  return first;
}

static void test_press(void) {
  pxt_gesture_state_t st;
  pxt_gesture_init(&st);
  uint32_t t = 1000;
  CHECK(run(&st, true, &t, 120) == PXT_GESTURE_NONE);
  // A press only counts once the double-press window has passed.
  CHECK(run(&st, false, &t, PXT_DOUBLE_PRESS_MS - 20) == PXT_GESTURE_NONE);
  CHECK(run(&st, false, &t, 100) == PXT_GESTURE_PRESS);
}

static void test_double_press(void) {
  pxt_gesture_state_t st;
  pxt_gesture_init(&st);
  uint32_t t = 0;
  run(&st, true, &t, 100);
  CHECK(run(&st, false, &t, 150) == PXT_GESTURE_NONE);
  run(&st, true, &t, 100);
  CHECK(run(&st, false, &t, 500) == PXT_GESTURE_DOUBLE_PRESS);
}

static void test_hold_and_cancel(void) {
  pxt_gesture_state_t st;
  pxt_gesture_init(&st);
  uint32_t t = 0;
  // Released between 2 and 3 s: back.
  CHECK(run(&st, true, &t, 2500) == PXT_GESTURE_NONE);
  CHECK(run(&st, false, &t, 500) == PXT_GESTURE_HOLD);
  // Released once the countdown is up: cancelled, not back.
  CHECK(run(&st, true, &t, 5000) == PXT_GESTURE_NONE);
  CHECK(pxt_gesture_reset_countdown(&st, t) == 5);
  CHECK(run(&st, false, &t, 500) == PXT_GESTURE_RESET_CANCELLED);
  CHECK(pxt_gesture_reset_countdown(&st, t) == -1);
}

static void test_reset(void) {
  pxt_gesture_state_t st;
  pxt_gesture_init(&st);
  uint32_t t = 0;
  CHECK(run(&st, true, &t, 2900) == PXT_GESTURE_NONE);
  CHECK(pxt_gesture_reset_countdown(&st, t) == -1);  // not shown before 3 s
  CHECK(run(&st, true, &t, 200) == PXT_GESTURE_NONE);
  CHECK(pxt_gesture_reset_countdown(&st, t) == 7);
  CHECK(run(&st, true, &t, 7000) == PXT_GESTURE_RESET);
  // Still holding after the reset fired: nothing more, then back to idle.
  CHECK(run(&st, true, &t, 3000) == PXT_GESTURE_NONE);
  CHECK(run(&st, false, &t, 500) == PXT_GESTURE_NONE);
  run(&st, true, &t, 100);
  CHECK(run(&st, false, &t, 500) == PXT_GESTURE_PRESS);
}

static void test_clock_wrap(void) {
  pxt_gesture_state_t st;
  pxt_gesture_init(&st);
  uint32_t t = 0xFFFFFF00u;  // millisecond counter about to wrap
  run(&st, true, &t, 100);
  CHECK(run(&st, false, &t, 500) == PXT_GESTURE_PRESS);
}

static void test_brightness(void) {
  pxt_brightness_t b;
  pxt_brightness_init(&b, 50);
  int level = -1;
  CHECK(pxt_brightness_turn(&b, 3, 1000) == 65);
  CHECK(pxt_brightness_turn(&b, -1, 1100) == 60);
  CHECK(pxt_brightness_overlay_visible(&b, 1200));
  // The server echoing the old value doesn't undo the turn...
  CHECK(!pxt_brightness_from_server(&b, 50));
  CHECK(b.level == 60);
  // ...and the report goes out once the knob is still.
  CHECK(!pxt_brightness_take_report(&b, 1300, &level));
  CHECK(pxt_brightness_take_report(&b, 1600, &level) && level == 60);
  CHECK(!pxt_brightness_take_report(&b, 1700, &level));
  CHECK(pxt_brightness_from_server(&b, 20) && b.level == 20);
  CHECK(!pxt_brightness_overlay_visible(&b, 5000));
  CHECK(pxt_brightness_turn(&b, -10, 6000) == 0);
  CHECK(pxt_brightness_turn(&b, 40, 6100) == 100);
}

static void test_codeword(void) {
  const uint8_t mac[6] = {0x24, 0x6f, 0x28, 0x12, 0x34, 0x56};
  const uint8_t other[6] = {0x24, 0x6f, 0x28, 0x12, 0x34, 0x57};
  char word[16], word2[16], url[32];
  pxt_code_word(mac, word, sizeof(word));
  pxt_code_word(mac, word2, sizeof(word2));
  CHECK(strcmp(word, word2) == 0);  // stable for one device
  CHECK(strchr(word, '-') != NULL && strlen(word) <= 7);
  pxt_setup_url(mac, url, sizeof(url));
  CHECK(strncmp(url, PXT_SETUP_URL_PREFIX, strlen(PXT_SETUP_URL_PREFIX)) == 0);
  CHECK(strlen(url) <= 25);  // must fit a version 1 alphanumeric QR code
  char url2[32];
  pxt_setup_url(other, url2, sizeof(url2));
  CHECK(strcmp(url, url2) != 0);
  printf("example: %s  %s\n", word, url);
}

static void test_screens(void) {
  static pxt_fb_t fb;
  const uint8_t mac[6] = {0x24, 0x6f, 0x28, 0x12, 0x34, 0x56};
  char word[16], url[32];
  pxt_code_word(mac, word, sizeof(word));
  pxt_setup_url(mac, url, sizeof(url));
  CHECK(pxt_screen_setup(&fb, url, word));
  // Too long for a version 1 code: refused rather than drawn wrong.
  CHECK(!pxt_screen_setup(&fb, "HTTPS://PIXELTRON.EXAMPLE/SETUP/ABCDEF", word));
  // Text drawing never writes outside the framebuffer.
  pxt_fb_clear(&fb);
  pxt_fb_text(&fb, 60, 30, "OVERFLOW", (pxt_rgb_t){255, 255, 255},
              PXT_FONT_5X7, 2);
  CHECK(pxt_fb_text_width("NO WIFI", PXT_FONT_5X7, 1) == 41);
  CHECK(pxt_fb_text_width("LET GO TO CANCEL", PXT_FONT_3X5, 1) <= PXT_FB_W);
}

int main(void) {
  test_press();
  test_double_press();
  test_hold_and_cancel();
  test_reset();
  test_clock_wrap();
  test_brightness();
  test_codeword();
  test_screens();
  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
