#include <string.h>

#include "font5x7.h"
#include "pxt_screens.h"

// 3x5 font for small print. Each glyph is five 3-bit rows, top row in the
// high bits, leftmost pixel in the high bit of each row.
#define ROWS(a, b, c, d, e) \
  (uint16_t)((0##a << 12) | (0##b << 9) | (0##c << 6) | (0##d << 3) | 0##e)

typedef struct {
  char ch;
  uint16_t bits;
} glyph3x5_t;

// Rows written as octal digits of binary triples: 7 = ###, 5 = #.#, 2 = .#.
static const glyph3x5_t kFont3x5[] = {
    {' ', 0},
    {'A', ROWS(2, 5, 7, 5, 5)},
    {'B', ROWS(6, 5, 6, 5, 6)},
    {'C', ROWS(3, 4, 4, 4, 3)},
    {'D', ROWS(6, 5, 5, 5, 6)},
    {'E', ROWS(7, 4, 6, 4, 7)},
    {'F', ROWS(7, 4, 6, 4, 4)},
    {'G', ROWS(3, 4, 5, 5, 3)},
    {'H', ROWS(5, 5, 7, 5, 5)},
    {'I', ROWS(7, 2, 2, 2, 7)},
    {'J', ROWS(1, 1, 1, 5, 2)},
    {'K', ROWS(5, 5, 6, 5, 5)},
    {'L', ROWS(4, 4, 4, 4, 7)},
    {'M', ROWS(5, 7, 7, 5, 5)},
    {'N', ROWS(6, 5, 5, 5, 5)},
    {'O', ROWS(2, 5, 5, 5, 2)},
    {'P', ROWS(6, 5, 6, 4, 4)},
    {'Q', ROWS(2, 5, 5, 6, 3)},
    {'R', ROWS(6, 5, 6, 5, 5)},
    {'S', ROWS(3, 4, 2, 1, 6)},
    {'T', ROWS(7, 2, 2, 2, 2)},
    {'U', ROWS(5, 5, 5, 5, 7)},
    {'V', ROWS(5, 5, 5, 5, 2)},
    {'W', ROWS(5, 5, 7, 7, 5)},
    {'X', ROWS(5, 5, 2, 5, 5)},
    {'Y', ROWS(5, 5, 2, 2, 2)},
    {'Z', ROWS(7, 1, 2, 4, 7)},
    {'0', ROWS(7, 5, 5, 5, 7)},
    {'1', ROWS(2, 6, 2, 2, 7)},
    {'2', ROWS(6, 1, 2, 4, 7)},
    {'3', ROWS(6, 1, 2, 1, 6)},
    {'4', ROWS(5, 5, 7, 1, 1)},
    {'5', ROWS(7, 4, 6, 1, 6)},
    {'6', ROWS(3, 4, 7, 5, 7)},
    {'7', ROWS(7, 1, 2, 2, 2)},
    {'8', ROWS(7, 5, 7, 5, 7)},
    {'9', ROWS(7, 5, 7, 1, 6)},
    {'-', ROWS(0, 0, 7, 0, 0)},
    {'.', ROWS(0, 0, 0, 0, 2)},
    {'\'', ROWS(2, 2, 0, 0, 0)},
    {'/', ROWS(1, 1, 2, 4, 4)},
    {':', ROWS(0, 2, 0, 2, 0)},
    {'!', ROWS(2, 2, 2, 0, 2)},
    {'?', ROWS(6, 1, 2, 0, 2)},
    {'%', ROWS(5, 1, 2, 4, 5)},
};

static uint16_t glyph3x5(char ch) {
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
  for (size_t i = 0; i < sizeof(kFont3x5) / sizeof(kFont3x5[0]); i++) {
    if (kFont3x5[i].ch == ch) return kFont3x5[i].bits;
  }
  return ROWS(7, 7, 7, 7, 7);  // unknown: solid block, easy to spot
}

void pxt_fb_clear(pxt_fb_t* fb) { memset(fb, 0, sizeof(*fb)); }

void pxt_fb_set(pxt_fb_t* fb, int x, int y, pxt_rgb_t c) {
  if (x < 0 || y < 0 || x >= PXT_FB_W || y >= PXT_FB_H) return;
  fb->px[y][x][0] = c.r;
  fb->px[y][x][1] = c.g;
  fb->px[y][x][2] = c.b;
}

void pxt_fb_fill(pxt_fb_t* fb, int x, int y, int w, int h, pxt_rgb_t c) {
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) pxt_fb_set(fb, xx, yy, c);
  }
}

static void block(pxt_fb_t* fb, int x, int y, int scale, pxt_rgb_t c) {
  pxt_fb_fill(fb, x, y, scale, scale, c);
}

int pxt_fb_text_width(const char* text, pxt_font_t font, int scale) {
  int n = (int)strlen(text);
  if (n == 0) return 0;
  int advance = font == PXT_FONT_3X5 ? 4 : 6;
  return (n * advance - 1) * scale;
}

int pxt_fb_text(pxt_fb_t* fb, int x, int y, const char* text, pxt_rgb_t c,
                pxt_font_t font, int scale) {
  int cx = x;
  for (const char* p = text; *p; p++) {
    if (font == PXT_FONT_3X5) {
      uint16_t bits = glyph3x5(*p);
      for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
          if (bits & (1u << ((4 - row) * 3 + (2 - col)))) {
            block(fb, cx + col * scale, y + row * scale, scale, c);
          }
        }
      }
      cx += 4 * scale;
    } else {
      unsigned char ch = (unsigned char)*p;
      if (ch < PXT_FONT5X7_FIRST_CHAR || ch > PXT_FONT5X7_LAST_CHAR) ch = '?';
      const uint8_t* cols = pxt_font5x7[ch - PXT_FONT5X7_FIRST_CHAR];
      for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
          if (cols[col] & (1u << row)) {
            block(fb, cx + col * scale, y + row * scale, scale, c);
          }
        }
      }
      cx += 6 * scale;
    }
  }
  return cx - x - scale;
}
