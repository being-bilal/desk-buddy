#include "faces.h"
#include "config.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

// Monochrome colour constants, spelled out so the engine is not tied to the
// SSD1306 header (swap in an SH1106/U8g2 driver without touching this file).
#define GFX_ON   1
#define GFX_OFF  0

// ---------------------------------------------------------------------------
//  Per-expression geometry
// ---------------------------------------------------------------------------
struct FaceSpec {
  int16_t eyeW, eyeH;      // rounded-rect eyes
  int16_t pupilW, pupilH;
  int16_t radius;
  float   baseLid;         // 0..1 of the eye hidden behind a droopy eyelid
  bool    arcEyes;         // happy closed "^" eyes instead of rects
  int16_t browSlant;       // >0 = angry inward-down brows
  bool    mouthSmile;
  bool    mouthO;
};

static const FaceSpec kSpecs[FACE_COUNT] = {
  /* NEUTRAL   */ { 30, 32, 12, 15, 9,  0.00f, false, 0, false, false },
  /* HAPPY     */ { 30, 20,  0,  0, 0,  0.00f, true,  0, true,  false },
  /* TIRED     */ { 30, 24, 12, 11, 8,  0.42f, false, 0, false, false },
  /* ANGRY     */ { 30, 28, 10, 10, 4,  0.20f, false, 1, false, false },
  /* SURPRISED */ { 26, 40,  9, 11, 13, 0.00f, false, 0, false, true  },
};

static const char *kNames[FACE_COUNT] = { "neutral", "happy", "tired",
                                          "angry",   "surprised" };

// Left / right eye centres.
static const int16_t kEyeY  = 33;
static const int16_t kEyeLX = 44;
static const int16_t kEyeRX = 84;

const char *faceName(FaceId f) { return (f < FACE_COUNT) ? kNames[f] : "neutral"; }

int faceFromName(const char *name) {
  if (!name) return -1;
  for (uint8_t i = 0; i < FACE_COUNT; i++)
    if (strcasecmp(name, kNames[i]) == 0) return (int)i;
  return -1;
}

// ---------------------------------------------------------------------------
//  Setup / state
// ---------------------------------------------------------------------------
void FaceEngine::begin(Adafruit_GFX *gfx) {
  _gfx = gfx;
  _face = FACE_NEUTRAL;
  _px = _py = _tx = _ty = 0;
  _nextGaze = millis() + 1200;
  _nextBlink = millis() + random(1800, 4200);
}

void FaceEngine::setFace(FaceId f) {
  if (f >= FACE_COUNT) f = FACE_NEUTRAL;
  if (f == _face) return;
  _face = f;
  _blinking = false;
  _blinkAmt = 0;
  _nextBlink = millis() + random(1500, 4000);
}

void FaceEngine::nextFace() {
  setFace((FaceId)((_face + 1) % FACE_COUNT));
}

// ---------------------------------------------------------------------------
//  Animation
// ---------------------------------------------------------------------------
void FaceEngine::update(uint32_t now) {
  // ---- gaze wander --------------------------------------------------------
  if ((int32_t)(now - _nextGaze) >= 0) {
    _tx = (float)random(-5, 6);
    _ty = (float)random(-3, 4);
    _nextGaze = now + random(1400, 4200);
  }
  _px += (_tx - _px) * 0.18f;
  _py += (_ty - _py) * 0.18f;

  // ---- blinking -----------------------------------------------------------
  if (!_blinking && (int32_t)(now - _nextBlink) >= 0) {
    _blinking = true;
    _blinkStart = now;
  }
  if (_blinking) {
    const uint32_t CLOSE = 70, OPEN = 100, TOTAL = CLOSE + OPEN;
    uint32_t e = now - _blinkStart;
    if (e >= TOTAL) {
      _blinking = false;
      _blinkAmt = 0;
      _nextBlink = now + random(2200, 6000);
    } else if (e < CLOSE) {
      _blinkAmt = (float)e / (float)CLOSE;
    } else {
      _blinkAmt = 1.0f - (float)(e - CLOSE) / (float)OPEN;
    }
  }
  _lastUpdate = now;
}

// ---------------------------------------------------------------------------
//  Drawing primitives
// ---------------------------------------------------------------------------
// Thick parabolic arc built from overlapping filled circles, so it works on
// any Adafruit_GFX version regardless of circle-helper quirks.
void FaceEngine::thickArc(Adafruit_GFX *g, int16_t x0, int16_t y0, int16_t w,
                          int16_t h, uint8_t thickness, bool up) {
  const int steps = w;
  const int r = thickness / 2;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;          // 0..1
    int16_t x = x0 + (int16_t)(t * (w - 1));
    float curve = 1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f);  // 0..1..0
    int16_t y = up ? (int16_t)(y0 + h - curve * h)
                   : (int16_t)(y0 + curve * h);
    g->fillCircle(x, y, r, GFX_ON);
  }
}

void FaceEngine::drawArcEye(int16_t cx, int16_t cy, const FaceSpec &s) {
  // An upward-bulging "∩" reads as a happy, closed eye.
  thickArc(_gfx, cx - s.eyeW / 2, cy - s.eyeH / 2, s.eyeW, s.eyeH, 5, false);
}

void FaceEngine::drawRectEye(int16_t cx, int16_t cy, const FaceSpec &s, float lid) {
  const int16_t w = s.eyeW, h = s.eyeH;
  const int16_t x = cx - w / 2, y = cy - h / 2;
  const int16_t lidH = (int16_t)(h * lid);

  // Fully closed -> a single thick line.
  if (h - lidH <= 5) {
    _gfx->fillRect(x, cy - 2, w, 4, GFX_ON);
    return;
  }

  _gfx->fillRoundRect(x, y, w, h, s.radius, GFX_ON);

  if (lidH > 0)  // erase the top slice to make a droopy eyelid
    _gfx->fillRect(x - 1, y - 1, w + 2, lidH + 1, GFX_OFF);

  if (s.pupilW <= 0) return;

  int16_t pcx = cx + (int16_t)lroundf(_px);
  int16_t pcy = cy + (int16_t)lroundf(_py);

  // Keep the pupil inside the remaining visible part of the eye.
  const int16_t visTop = y + lidH;
  const int16_t visBot = y + h;
  if (pcy - s.pupilH / 2 < visTop + 1) pcy = visTop + 1 + s.pupilH / 2;
  if (pcy + s.pupilH / 2 > visBot - 1) pcy = visBot - 1 - s.pupilH / 2;
  if (pcx - s.pupilW / 2 < x + 2)      pcx = x + 2 + s.pupilW / 2;
  if (pcx + s.pupilW / 2 > x + w - 2)  pcx = x + w - 2 - s.pupilW / 2;

  _gfx->fillRoundRect(pcx - s.pupilW / 2, pcy - s.pupilH / 2, s.pupilW,
                      s.pupilH, min((int16_t)4, (int16_t)(s.pupilH / 2)),
                      GFX_OFF);
}

void FaceEngine::drawBrow(int16_t cx, int16_t cy, const FaceSpec &s, bool left) {
  const int16_t half = s.eyeW / 2;
  const int16_t yTop = cy - s.eyeH / 2 - 5;
  // Inner end dips down towards the nose -> cross/angry.
  const int16_t innerX = left ? cx + half - 3 : cx - half + 3;
  const int16_t outerX = left ? cx - half + 1 : cx + half - 1;
  const int16_t innerY = yTop + s.browSlant * 4;
  for (int8_t t = -1; t <= 1; t++)
    _gfx->drawLine(outerX, yTop + t, innerX, innerY + t, GFX_ON);
}

void FaceEngine::drawMouth(const FaceSpec &s) {
  if (s.mouthSmile) {
    thickArc(_gfx, 52, 48, 24, 10, 3, true);
  } else if (s.mouthO) {
    _gfx->drawCircle(64, 55, 4, GFX_ON);
  }
}

// ---------------------------------------------------------------------------
//  Frame
// ---------------------------------------------------------------------------
void FaceEngine::render() {
  if (!_gfx) return;

  const FaceSpec &s = kSpecs[_face];
  const float lid = s.baseLid + (1.0f - s.baseLid) * _blinkAmt;

  if (s.arcEyes) {
    if (_blinkAmt > 0.5f) {
      _gfx->fillRect(kEyeLX - s.eyeW / 2, kEyeY - 2, s.eyeW, 4, GFX_ON);
      _gfx->fillRect(kEyeRX - s.eyeW / 2, kEyeY - 2, s.eyeW, 4, GFX_ON);
    } else {
      drawArcEye(kEyeLX, kEyeY, s);
      drawArcEye(kEyeRX, kEyeY, s);
    }
  } else {
    drawRectEye(kEyeLX, kEyeY, s, lid);
    drawRectEye(kEyeRX, kEyeY, s, lid);
  }

  if (s.browSlant > 0) {
    drawBrow(kEyeLX, kEyeY, s, true);
    drawBrow(kEyeRX, kEyeY, s, false);
  }

  drawMouth(s);
}
