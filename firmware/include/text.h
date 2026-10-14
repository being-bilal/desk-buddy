// ---------------------------------------------------------------------------
//  Text screen - what a long press shows.
//  The Adafruit GFX default font is ASCII-only, so incoming UTF-8 is folded
//  to ASCII (accents mapped, everything else replaced) before wrapping.
// ---------------------------------------------------------------------------
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

#define TEXT_MAX_CHARS   512
#define TEXT_COLS        20      // 20 * 6px = 120px at text size 1
#define TEXT_VISIBLE_ROWS 7
#define TEXT_MAX_ROWS    40

class TextScreen {
public:
  void setText(const char *utf8);        // sanitise + wrap + persist by caller
  const char *raw() const { return _raw; }
  bool empty() const { return _raw[0] == '\0'; }

  void render(Adafruit_GFX *gfx, uint32_t elapsedMs) const;
  uint32_t durationMs() const;

private:
  char    _raw[TEXT_MAX_CHARS + 1] = {0};
  char    _rows[TEXT_MAX_ROWS][TEXT_COLS + 1];
  uint8_t _rowCount = 0;

  void rewrap();
};
