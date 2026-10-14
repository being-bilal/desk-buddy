// ---------------------------------------------------------------------------
//  Face engine - five animated expressions on a 128x64 mono OLED.
// ---------------------------------------------------------------------------
#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

enum FaceId : uint8_t {
  FACE_NEUTRAL = 0,
  FACE_HAPPY,
  FACE_TIRED,
  FACE_ANGRY,
  FACE_SURPRISED,
  FACE_COUNT
};

const char *faceName(FaceId f);
int faceFromName(const char *name);   // -1 when unknown

class FaceEngine {
public:
  void begin(Adafruit_GFX *gfx);

  void   setFace(FaceId f);
  FaceId face() const { return _face; }
  void   nextFace();                  // single-tap action

  void update(uint32_t nowMs);        // advance animation
  void render();                      // draw current frame (no display() call)

private:
  void drawRectEye(int16_t cx, int16_t cy, const struct FaceSpec &s, float lid);
  void drawArcEye(int16_t cx, int16_t cy, const struct FaceSpec &s);
  void drawBrow(int16_t cx, int16_t cy, const struct FaceSpec &s, bool left);
  void drawMouth(const struct FaceSpec &s);
  static void thickArc(Adafruit_GFX *g, int16_t x0, int16_t y0, int16_t w,
                       int16_t h, uint8_t thickness, bool up);

  Adafruit_GFX *_gfx = nullptr;
  FaceId  _face = FACE_NEUTRAL;

  float   _px = 0, _py = 0;           // current pupil offset
  float   _tx = 0, _ty = 0;           // target pupil offset
  uint32_t _nextGaze = 0;

  bool     _blinking = false;
  uint32_t _blinkStart = 0;
  uint32_t _nextBlink = 0;
  float    _blinkAmt = 0;             // 0 open .. 1 shut

  uint32_t _lastUpdate = 0;
};
