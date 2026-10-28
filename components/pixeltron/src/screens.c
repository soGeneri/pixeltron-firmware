#include <stdio.h>
#include <string.h>

#include "pxt_screens.h"
#include "qrcodegen.h"

static const pxt_rgb_t kWhite = {200, 200, 200};
static const pxt_rgb_t kDim = {90, 90, 90};
static const pxt_rgb_t kAmber = {255, 170, 0};
static const pxt_rgb_t kRed = {230, 30, 30};
static const pxt_rgb_t kCyan = {0, 170, 230};
static const pxt_rgb_t kGreen = {40, 200, 80};
// QR light modules: not full white, to keep current and glare down.
static const pxt_rgb_t kQrLight = {170, 170, 170};

// Icons as rows of '#' (lit) and '.' (off).
static const char* const kIconWifi[] = {
    "...#####...", ".##.....##.", "#...###...#", "..##...##..",
    "....#.#....", ".....#.....",
};
static const char* const kIconGlobe[] = {
    "..#####..", ".#..#..#.", "#..#.#..#", "#########",
    "#..#.#..#", ".#..#..#.", "..#####..",
};
static const char* const kIconCloud[] = {
    "....###....", "..##...#...", ".#.....###.", "#.........#",
    "#.........#", ".#########.",
};
static const char* const kIconPlug[] = {
    ".#.#.", ".#.#.", "#####", "#####", ".###.", "..#..", "..#..",
};

static void draw_icon(pxt_fb_t* fb, const char* const* rows, int nrows, int x,
                      int y, pxt_rgb_t c) {
  for (int r = 0; r < nrows; r++) {
    for (int col = 0; rows[r][col]; col++) {
      if (rows[r][col] == '#') pxt_fb_set(fb, x + col, y + r, c);
    }
  }
}

// Red diagonal from top-right to bottom-left across a w x h box.
static void draw_slash(pxt_fb_t* fb, int x, int y, int w, int h) {
  int steps = w > h ? w : h;
  for (int i = 0; i <= steps; i++) {
    int px = x + w - (i * w) / steps;
    int py = y - 1 + (i * (h + 1)) / steps;
    pxt_fb_set(fb, px, py, kRed);
  }
}

static int centered_x(const char* text, pxt_font_t font, int scale) {
  return (PXT_FB_W - pxt_fb_text_width(text, font, scale)) / 2;
}

void pxt_screen_status(pxt_fb_t* fb, pxt_status_t status) {
  pxt_fb_clear(fb);
  const char* const* icon = NULL;
  int rows = 0;
  const char* title = "";
  bool slash = true;
  switch (status) {
    case PXT_STATUS_NO_WIFI:
      icon = kIconWifi, rows = 6, title = "NO WIFI";
      break;
    case PXT_STATUS_NO_INTERNET:
      icon = kIconGlobe, rows = 7, title = "NO NET";
      break;
    case PXT_STATUS_SERVER_OFFLINE:
      icon = kIconCloud, rows = 6, title = "OFFLINE";
      break;
    case PXT_STATUS_WEAK_POWER:
      icon = kIconPlug, rows = 7, title = "USE PLUG", slash = false;
      break;
    case PXT_STATUS_NOT_LINKED:
    default:
      title = "NOT LINKED";
      break;
  }
  if (icon) {
    int w = (int)strlen(icon[0]);
    int x = (PXT_FB_W - w) / 2;
    int y = 2 + (7 - rows) / 2;
    draw_icon(fb, icon, rows, x, y, status == PXT_STATUS_WEAK_POWER ? kAmber
                                                                     : kWhite);
    if (slash) draw_slash(fb, x, y, w, rows);
  }
  pxt_fb_text(fb, centered_x(title, PXT_FONT_5X7, 1), 13, title, kWhite,
              PXT_FONT_5X7, 1);
  char code[24];
  snprintf(code, sizeof(code), "E%d PXT.IO/HELP", (int)status);
  pxt_fb_text(fb, centered_x(code, PXT_FONT_3X5, 1), 25, code, kDim,
              PXT_FONT_3X5, 1);
}

bool pxt_screen_setup(pxt_fb_t* fb, const char* url, const char* code_word) {
  uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
  uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(1)];
  pxt_fb_clear(fb);
  if (!qrcodegen_encodeText(url, tmp, qr, qrcodegen_Ecc_LOW, 1, 1,
                            qrcodegen_Mask_AUTO, false)) {
    return false;
  }
  int size = qrcodegen_getSize(qr);  // 21
  const int quiet = 4;
  int box = size + 2 * quiet;  // 29
  int ox = 1, oy = (PXT_FB_H - box) / 2;
  pxt_fb_fill(fb, ox, oy, box, box, kQrLight);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (qrcodegen_getModule(qr, x, y)) {
        pxt_fb_set(fb, ox + quiet + x, oy + quiet + y, (pxt_rgb_t){0, 0, 0});
      }
    }
  }

  // Right of the code: "SCAN", then the code word split at the dash.
  const int left = ox + box + 1;
  const int width = PXT_FB_W - left;
  char word[8] = {0}, digits[8] = {0};
  const char* dash = strchr(code_word, '-');
  if (dash) {
    size_t n = (size_t)(dash - code_word);
    if (n >= sizeof(word)) n = sizeof(word) - 1;
    memcpy(word, code_word, n);
    snprintf(digits, sizeof(digits), "%s", dash + 1);
  } else {
    snprintf(word, sizeof(word), "%s", code_word);
  }
  const char* lines[] = {"SCAN", word, digits};
  const int ys[] = {3, 13, 22};
  for (int i = 0; i < 3; i++) {
    int w = pxt_fb_text_width(lines[i], PXT_FONT_5X7, 1);
    pxt_fb_text(fb, left + (width - w) / 2, ys[i], lines[i],
                i == 0 ? kDim : kAmber, PXT_FONT_5X7, 1);
  }
  return true;
}

void pxt_screen_updating(pxt_fb_t* fb, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  pxt_fb_clear(fb);
  pxt_fb_text(fb, centered_x("UPDATING", PXT_FONT_5X7, 1), 3, "UPDATING",
              kWhite, PXT_FONT_5X7, 1);
  const int bx = 6, by = 14, bw = 52, bh = 4;
  pxt_fb_fill(fb, bx, by, bw, bh, (pxt_rgb_t){30, 30, 30});
  pxt_fb_fill(fb, bx, by, bw * percent / 100, bh, kCyan);
  pxt_fb_text(fb, centered_x("DON'T UNPLUG", PXT_FONT_3X5, 1), 24,
              "DON'T UNPLUG", kAmber, PXT_FONT_3X5, 1);
}

void pxt_screen_reset_countdown(pxt_fb_t* fb, int seconds_left) {
  char num[4];
  snprintf(num, sizeof(num), "%d", seconds_left);
  pxt_fb_clear(fb);
  pxt_fb_text(fb, 4, 4, "RESET", kRed, PXT_FONT_5X7, 1);
  pxt_fb_text(fb, 4, 13, "IN", kDim, PXT_FONT_3X5, 1);
  int w = pxt_fb_text_width(num, PXT_FONT_5X7, 2);
  pxt_fb_text(fb, 60 - w, 2, num, kWhite, PXT_FONT_5X7, 2);
  pxt_fb_text(fb, centered_x("LET GO TO CANCEL", PXT_FONT_3X5, 1), 25,
              "LET GO TO CANCEL", kDim, PXT_FONT_3X5, 1);
}

void pxt_screen_resetting(pxt_fb_t* fb) {
  pxt_fb_clear(fb);
  pxt_fb_text(fb, centered_x("RESETTING", PXT_FONT_5X7, 1), 12, "RESETTING",
              kRed, PXT_FONT_5X7, 1);
}

// A ring of 8 dots with one bright "comet" running around it.
static void draw_spinner(pxt_fb_t* fb, int cx, int cy, int frame,
                         pxt_rgb_t c) {
  static const int kRing[8][2] = {{0, -4}, {3, -3}, {4, 0},  {3, 3},
                                  {0, 4},  {-3, 3}, {-4, 0}, {-3, -3}};
  for (int i = 0; i < 8; i++) {
    int age = (frame - i) & 7;  // 0 = head of the comet
    uint8_t k = age == 0 ? 255 : age == 1 ? 120 : age == 2 ? 60 : 25;
    pxt_rgb_t dot = {(uint8_t)(c.r * k / 255), (uint8_t)(c.g * k / 255),
                     (uint8_t)(c.b * k / 255)};
    pxt_fb_fill(fb, cx + kRing[i][0], cy + kRing[i][1], 1, 1, dot);
  }
}

void pxt_screen_lesson(pxt_fb_t* fb, int step, int frame) {
  pxt_fb_clear(fb);
  if (step == 0) {
    pxt_fb_text(fb, 3, 5, "TURN", kWhite, PXT_FONT_5X7, 1);
    pxt_fb_text(fb, 3, 15, "ME", kAmber, PXT_FONT_5X7, 1);
    draw_spinner(fb, 50, 12, frame, kAmber);
    pxt_fb_text(fb, 3, 25, "BRIGHTNESS", kDim, PXT_FONT_3X5, 1);
  } else {
    pxt_fb_text(fb, 3, 5, "PRESS", kWhite, PXT_FONT_5X7, 1);
    pxt_fb_text(fb, 3, 15, "ME", kAmber, PXT_FONT_5X7, 1);
    // Knob seen from above, pulsing as if pressed.
    bool down = (frame / 4) % 2;
    int r = down ? 4 : 5;
    for (int y = -r; y <= r; y++) {
      for (int x = -r; x <= r; x++) {
        int d = x * x + y * y;
        if (d <= r * r && d >= (r - 1) * (r - 1)) {
          pxt_fb_set(fb, 50 + x, 12 + y, kAmber);
        }
      }
    }
    if (down) pxt_fb_fill(fb, 49, 11, 3, 3, kAmber);
    pxt_fb_text(fb, 3, 25, "NEXT APP", kDim, PXT_FONT_3X5, 1);
  }
}

void pxt_overlay_brightness(pxt_fb_t* fb, int level) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  pxt_fb_fill(fb, 0, 25, PXT_FB_W, 7, (pxt_rgb_t){0, 0, 0});
  // Small sun: a dot with four rays.
  pxt_fb_fill(fb, 2, 27, 3, 3, kAmber);
  pxt_fb_set(fb, 3, 25, kAmber);
  pxt_fb_set(fb, 3, 31, kAmber);
  pxt_fb_set(fb, 0, 28, kAmber);
  pxt_fb_set(fb, 6, 28, kAmber);
  const int bx = 9, by = 27, bw = 40, bh = 3;
  pxt_fb_fill(fb, bx, by, bw, bh, (pxt_rgb_t){35, 35, 35});
  pxt_fb_fill(fb, bx, by, bw * level / 100, bh, kGreen);
  char pct[5];
  snprintf(pct, sizeof(pct), "%d", level);
  pxt_fb_text(fb, 63 - pxt_fb_text_width(pct, PXT_FONT_3X5, 1), 26, pct,
              kWhite, PXT_FONT_3X5, 1);
}
