#include "touch.h"
#include "config.h"

#include <Arduino.h>

static inline bool readTouchPin() {
#if TOUCH_ACTIVE_HIGH
  return digitalRead(PIN_TOUCH) == HIGH;
#else
  return digitalRead(PIN_TOUCH) == LOW;
#endif
}

void Touch::begin() {
#if TOUCH_ACTIVE_HIGH
  pinMode(PIN_TOUCH, INPUT);
#else
  pinMode(PIN_TOUCH, INPUT_PULLUP);
#endif
  _raw = _down = readTouchPin();
  _since = _lastEdge = millis();
}

TouchEvent Touch::poll(uint32_t now) {
  const bool sample = readTouchPin();

  // Accept a level change only once it has held steady past the debounce
  // window; cheap and reliable for a mechanical-free capacitive pad.
  if (sample != _raw && (now - _lastEdge) > TOUCH_DEBOUNCE_MS) {
    _raw = sample;
    _lastEdge = now;

    if (_raw && !_down) {                // press
      _down = true;
      _since = now;
      _longFired = false;
    } else if (!_raw && _down) {         // release
      _down = false;
      if (!_longFired) return TOUCH_TAP;
    }
  }

  if (_down && !_longFired && (now - _since) >= LONG_PRESS_MS) {
    _longFired = true;
    return TOUCH_LONG_PRESS;
  }
  return TOUCH_NONE;
}
