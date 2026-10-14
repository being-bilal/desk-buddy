#include "text.h"
#include "config.h"

#include <Arduino.h>
#include <string.h>

// ---------------------------------------------------------------------------
//  UTF-8 -> ASCII folding
// ---------------------------------------------------------------------------
// Most European desk-buddy messages are Latin-1; mapping those keeps accents
// readable instead of turning them into '?'. U+00C0..U+00FF sits in UTF-8 as
// 0xC3 0x8x/0x9x, so the codepoint is just 0xC0 | (b1 & 0x3F).
static char foldLatin1(uint8_t cp) {
  switch (cp) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return 'A';
    case 0xC7: return 'C';
    case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'E';
    case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'I';
    case 0xD1: return 'N';
    case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return 'O';
    case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'U';
    case 0xDD: return 'Y';
    case 0xDF: return 's';
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return 'a';
    case 0xE7: return 'c';
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';
    case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i';
    case 0xF1: return 'n';
    case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return 'o';
    case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u';
    case 0xFD: case 0xFF: return 'y';
    default:   return '?';
  }
}

static void sanitise(const char *in, char *out, size_t outSize) {
  size_t o = 0;
  for (size_t i = 0; in[i] && o + 1 < outSize;) {
    const uint8_t c = (uint8_t)in[i];

    if (c < 0x80) {                       // plain ASCII
      if (c == '\n' || c == '\t' || (c >= 0x20 && c <= 0x7E)) {
        out[o++] = (c == '\t') ? ' ' : (char)c;
      }
      i++;
      continue;
    }

    if (c == 0xC3 && in[i + 1]) {         // Latin-1 supplement
      out[o++] = foldLatin1((uint8_t)(0xC0 | (in[i + 1] & 0x3F)));
      i += 2;
      continue;
    }
    if (c == 0xC5 && in[i + 1]) {         // OE / oe ligatures
      const uint8_t b = (uint8_t)in[i + 1];
      out[o++] = (b == 0x92) ? 'E' : (b == 0x93) ? 'e' : '?';
      i += 2;
      continue;
    }

    // Anything else (emoji, CJK, curly quotes) has no glyph in the built-in
    // font: skip the whole sequence silently.
    size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
    i += len;
  }
  out[o] = '\0';
}

// ---------------------------------------------------------------------------
void TextScreen::setText(const char *utf8) {
  if (!utf8) utf8 = "";
  sanitise(utf8, _raw, sizeof(_raw));
  rewrap();
}

void TextScreen::rewrap() {
  _rowCount = 0;
  char line[TEXT_COLS + 1];
  uint8_t li = 0;
  const char *p = _raw;

  auto flush = [&]() {
    if (li == 0) return;
    line[li] = '\0';
    if (_rowCount < TEXT_MAX_ROWS) {
      strncpy(_rows[_rowCount], line, TEXT_COLS);
      _rows[_rowCount][TEXT_COLS] = '\0';
      _rowCount++;
    }
    li = 0;
  };

  while (*p) {
    if (*p == '\n') { flush(); p++; continue; }

    // Grab the next word.
    char word[TEXT_COLS + 1];
    uint8_t wi = 0;
    while (*p && *p != ' ' && *p != '\n' && wi < TEXT_COLS) word[wi++] = *p++;

    // Skip the run of spaces.
    while (*p == ' ') p++;
    word[wi] = '\0';

    if (wi == 0) continue;

    const uint8_t need = (li == 0) ? wi : (uint8_t)(wi + 1);
    if (li + need > TEXT_COLS) {
      flush();
      if (wi > TEXT_COLS) wi = TEXT_COLS;
      memcpy(line, word, wi);
      li = wi;
    } else {
      if (li > 0) line[li++] = ' ';
      memcpy(line + li, word, wi);
      li += wi;
    }

    // A word longer than a line gets hard-split.
    if (li >= TEXT_COLS) flush();
  }
  flush();

  if (_rowCount == 0) {   // keep at least one (empty) row
    _rows[0][0] = '\0';
    _rowCount = 1;
  }
}

// ---------------------------------------------------------------------------
uint32_t TextScreen::durationMs() const {
  if (_rowCount <= TEXT_VISIBLE_ROWS) return TEXT_MIN_MS;
  return TEXT_MIN_MS + (uint32_t)(_rowCount - TEXT_VISIBLE_ROWS) * TEXT_PER_LINE_MS;
}

void TextScreen::render(Adafruit_GFX *gfx, uint32_t elapsedMs) const {
  if (!gfx) return;

  const uint8_t extra = (_rowCount > TEXT_VISIBLE_ROWS)
                          ? (uint8_t)(_rowCount - TEXT_VISIBLE_ROWS) : 0;

  // Hold at the top, then step down one row at a time.
  uint8_t offset = 0;
  if (extra > 0) {
    const uint32_t HOLD = 1100;
    if (elapsedMs > HOLD)
      offset = (uint8_t)((elapsedMs - HOLD) / TEXT_PER_LINE_MS);
    if (offset > extra) offset = extra;
  }

  gfx->setTextSize(1);
  gfx->setTextColor(1);

  const uint8_t rows = (_rowCount < TEXT_VISIBLE_ROWS) ? _rowCount : TEXT_VISIBLE_ROWS;
  int16_t y = 6;
  if (_rowCount <= TEXT_VISIBLE_ROWS) {
    // Centre short messages vertically.
    y = (int16_t)((OLED_HEIGHT - _rowCount * 9) / 2 + 1);
  }

  for (uint8_t r = 0; r < rows; r++) {
    const char *row = _rows[r + offset];
    if (row[0]) {
      const int16_t w = (int16_t)strlen(row) * 6;
      const int16_t x = (OLED_WIDTH - w) / 2;
      gfx->setCursor(x < 0 ? 0 : x, y + (int16_t)r * 9);
      gfx->print(row);
    }
  }
}
