// Pixeltron's own panel screens: setup QR, status/error screens, firmware
// update progress, reset countdown, brightness overlay and the knob lesson.
//
// Everything draws into a 64x32 RGB888 framebuffer (row-major, 3 bytes per
// pixel), which the device hands to display_draw() and the simulator shows
// directly. Layouts and wording follow docs/08-setup-experience.md.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PXT_FB_W 64
#define PXT_FB_H 32

// Largest setup QR code: version 3 is 29 modules, the most that fits the
// 32-pixel-high panel with a light border.
#define PXT_QR_MAX_VERSION 3

typedef struct {
  uint8_t px[PXT_FB_H][PXT_FB_W][3];
} pxt_fb_t;

typedef struct {
  uint8_t r, g, b;
} pxt_rgb_t;

typedef enum {
  PXT_FONT_5X7 = 0,
  PXT_FONT_3X5,
} pxt_font_t;

void pxt_fb_clear(pxt_fb_t* fb);
void pxt_fb_set(pxt_fb_t* fb, int x, int y, pxt_rgb_t c);
void pxt_fb_fill(pxt_fb_t* fb, int x, int y, int w, int h, pxt_rgb_t c);
// Draws upper-case text and returns its width in pixels.
int pxt_fb_text(pxt_fb_t* fb, int x, int y, const char* text, pxt_rgb_t c,
                pxt_font_t font, int scale);
int pxt_fb_text_width(const char* text, pxt_font_t font, int scale);

// Status codes shown on the panel, matching the app's status cards and the
// help articles at tenthousandpixels.com/e/<code>.
typedef enum {
  PXT_STATUS_NO_WIFI = 1,     // E1: saved network not visible / auth fails
  PXT_STATUS_NO_INTERNET,     // E2: Wi-Fi up, internet unreachable
  PXT_STATUS_SERVER_OFFLINE,  // E3: internet up, server down or 5xx
  PXT_STATUS_NOT_LINKED,      // E4: claim revoked, shows the setup QR
  PXT_STATUS_WEAK_POWER,      // E5: power source advertises < 3 A
} pxt_status_t;

void pxt_screen_status(pxt_fb_t* fb, pxt_status_t status);

// Setup screen: a QR code of `url` beside the code word, e.g. "FOX-42". `url`
// is normally the Wi-Fi payload from pxt_wifi_qr(); anything up to a version
// PXT_QR_MAX_VERSION code fits (53 bytes, or 77 upper-case characters such
// as the setup URL). Returns false if it doesn't.
bool pxt_screen_setup(pxt_fb_t* fb, const char* url, const char* code_word);

void pxt_screen_updating(pxt_fb_t* fb, int percent);
void pxt_screen_reset_countdown(pxt_fb_t* fb, int seconds_left);
void pxt_screen_resetting(pxt_fb_t* fb);

// Knob lesson at the end of setup. step 0 = "turn me", 1 = "press me".
// `frame` animates the arrow; advance it every ~100 ms.
void pxt_screen_lesson(pxt_fb_t* fb, int step, int frame);

// Draws a brightness bar over the bottom rows of whatever is in fb.
void pxt_overlay_brightness(pxt_fb_t* fb, int level);

#ifdef __cplusplus
}
#endif
