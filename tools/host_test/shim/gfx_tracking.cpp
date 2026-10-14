// Shared bookkeeping for the shim, so each test binary does not have to
// redefine the globals.

#include "Adafruit_GFX.h"

std::vector<GfxCall> g_calls;
DrawStats            g_stats;

void DrawStats::reset() {
  minX = 32767;
  minY = 32767;
  maxX = -32768;
  maxY = -32768;
  calls = 0;
  outOfBounds = false;
  hash = 2166136261u;   // FNV-ish seed
}

void gfxNote(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  if (x0 > x1) { const int16_t t = x0; x0 = x1; x1 = t; }
  if (y0 > y1) { const int16_t t = y0; y0 = y1; y1 = t; }

  g_stats.calls++;

  if (x0 < g_stats.minX) g_stats.minX = x0;
  if (y0 < g_stats.minY) g_stats.minY = y0;
  if (x1 > g_stats.maxX) g_stats.maxX = x1;
  if (y1 > g_stats.maxY) g_stats.maxY = y1;

  // The panel is 128x64; anything outside would be silently clipped on real
  // hardware, which is exactly the kind of bug a picture-less test can catch.
  if (x0 < 0 || y0 < 0 || x1 > 127 || y1 > 63) g_stats.outOfBounds = true;

  // Order-sensitive signature of the whole frame.
  g_stats.hash = (g_stats.hash ^ (uint32_t)(x0 * 7 + y0 * 13 + x1 * 17 + y1 * 19))
                 * 16777619u;
}
