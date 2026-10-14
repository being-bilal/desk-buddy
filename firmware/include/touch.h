// ---------------------------------------------------------------------------
//  Touch - one digital input from a TTP223-style capacitive module.
//
//  The ESP32-C3 has no on-chip capacitive-touch peripheral, so the gesture
//  logic runs in software on top of a plain digital pin:
//     tap        -> next face
//     long press -> show the stored message + play track 2
// ---------------------------------------------------------------------------
#pragma once

#include <stdint.h>

enum TouchEvent : uint8_t {
  TOUCH_NONE = 0,
  TOUCH_TAP,
  TOUCH_LONG_PRESS
};

class Touch {
public:
  void begin();
  TouchEvent poll(uint32_t nowMs);

  bool     pressed() const { return _down; }
  uint32_t heldMs(uint32_t nowMs) const { return _down ? (nowMs - _since) : 0; }

private:
  bool     _raw = false;      // debounced electrical state
  bool     _down = false;     // reported state
  bool     _longFired = false;
  uint32_t _since = 0;
  uint32_t _lastEdge = 0;
};
