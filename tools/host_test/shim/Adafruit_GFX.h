// Minimal Adafruit_GFX stand-in: records what TextScreen draws so the host
// test can assert on real rendered output instead of on internals.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct GfxCall {
  int16_t x;
  int16_t y;
  std::string text;
};

extern std::vector<GfxCall> g_calls;   // defined in test_text.cpp

class Adafruit_GFX {
public:
  Adafruit_GFX(int16_t w, int16_t h) : _w(w), _h(h) {}

  void setTextSize(uint8_t) {}
  void setTextColor(uint16_t) {}
  void setCursor(int16_t x, int16_t y) { _cx = x; _cy = y; }
  void print(const char *s) { g_calls.push_back({_cx, _cy, std::string(s ? s : "")}); }

  void clearDisplay() {}
  int16_t width() const { return _w; }
  int16_t height() const { return _h; }

private:
  int16_t _w, _h;
  int16_t _cx = 0, _cy = 0;
};
