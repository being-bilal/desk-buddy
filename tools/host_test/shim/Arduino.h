// Minimal host shim so the text-wrapping logic can be unit-tested off-device.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static inline uint32_t millis() { return 0; }

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
