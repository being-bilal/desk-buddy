// Minimal Adafruit_GFX stand-in.
//
// It does not rasterise. Every primitive reports its bounding box through
// gfxNote(), which is enough to prove two things about the face engine without
// a real display:
//   * nothing is drawn outside the 128x64 panel (Adafruit would silently clip)
//   * every expression produces distinct geometry
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct GfxCall {
  int16_t x;
  int16_t y;
  std::string text;
};

struct DrawStats {
  int32_t  minX, minY, maxX, maxY;
  int      calls;
  bool     outOfBounds;
  uint32_t hash;
  void reset();
};

extern std::vector<GfxCall> g_calls;   // defined in shim/gfx_tracking.cpp
extern DrawStats            g_stats;

// Called by every primitive with its bounding box, inclusive.
void gfxNote(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

static inline int16_t lo2(int16_t a, int16_t b) { return a < b ? a : b; }
static inline int16_t hi2(int16_t a, int16_t b) { return a > b ? a : b; }
static inline int16_t lo3(int16_t a, int16_t b, int16_t c) {
  return lo2(lo2(a, b), c);
}
static inline int16_t hi3(int16_t a, int16_t b, int16_t c) {
  return hi2(hi2(a, b), c);
}

class Adafruit_GFX {
public:
  Adafruit_GFX(int16_t w, int16_t h) : _w(w), _h(h) {}

  // ---------------------------- text -------------------------------------
  void setTextSize(uint8_t) {}
  void setTextColor(uint16_t) {}
  void setCursor(int16_t x, int16_t y) { _cx = x; _cy = y; }
  void print(const char *s) {
    const char *t = s ? s : "";
    const int16_t n = (int16_t)std::strlen(t);
    g_calls.push_back({_cx, _cy, std::string(t)});
    gfxNote(_cx, _cy, (int16_t)(_cx + n * 6 - 1), (int16_t)(_cy + 7));
  }

  void clearDisplay() {}
  int16_t width() const { return _w; }
  int16_t height() const { return _h; }

  // --------------------------- drawing -----------------------------------
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t) {
    gfxNote(x, y, (int16_t)(x + w - 1), (int16_t)(y + h - 1));
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t,
                     uint16_t) {
    gfxNote(x, y, (int16_t)(x + w - 1), (int16_t)(y + h - 1));
  }
  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t) {
    gfxNote((int16_t)(x - r), (int16_t)(y - r), (int16_t)(x + r),
            (int16_t)(y + r));
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t) {
    gfxNote((int16_t)(x - r), (int16_t)(y - r), (int16_t)(x + r),
            (int16_t)(y + r));
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t) {
    gfxNote(lo2(x0, x1), lo2(y0, y1), hi2(x0, x1), hi2(y0, y1));
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2, uint16_t) {
    gfxNote(lo3(x0, x1, x2), lo3(y0, y1, y2), hi3(x0, x1, x2), hi3(y0, y1, y2));
  }

private:
  int16_t _w, _h;
  int16_t _cx = 0, _cy = 0;
};
