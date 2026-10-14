// Minimal host shim so the OLED logic can be unit-tested off-device.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static inline uint32_t millis() { return 0; }

// Deterministic stand-ins so rendering is reproducible in tests.
static inline long random(long howbig) { (void)howbig; return 0; }
static inline long random(long howsmall, long howbig) {
  (void)howbig;
  return howsmall;
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
