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
enum EyeStyle : uint8_t {
  EYE_RECT,     // rounded rectangle + pupil (the default)
  EYE_ARC,      // closed "∩" eye
  EYE_HEART,    // heart-shaped
  EYE_CROSS,    // "X"
  EYE_SHADES    // one bar across both eyes
};

enum MouthStyle : uint8_t {
  MOUTH_NONE,
  MOUTH_SMILE,  // "∪"
  MOUTH_FROWN,  // "∩"
  MOUTH_O,      // small open circle
  MOUTH_FLAT,   // short dash
  MOUTH_GRIN,   // open mouth with a tooth line
  MOUTH_SMIRK   // short off-centre "∪"
};

struct FaceSpec {
  int16_t    eyeW, eyeH;
  int16_t    pupilW, pupilH;
  int16_t    radius;
  float      baseLid;     // 0..1 of the eye hidden behind a droopy eyelid
  EyeStyle   eye;
  MouthStyle mouth;
  bool       brows;
  int8_t     browSlant;   // +1 inner end dips (angry), -1 inner end rises (sad)
  int8_t     browLiftL;   // px, negative raises
  int8_t     browLiftR;
  bool       winkLeft;    // draw the left eye as an arc instead
  bool       blush;
};

static const FaceSpec kSpecs[FACE_COUNT] = {
  //            eyeW eyeH puW puH  rad  lid    eye        mouth        brows slant  liftL liftR  wink  blush
  /* NEUTRAL   */{ 30, 32, 12, 15,  9, 0.00f, EYE_RECT,   MOUTH_NONE,  false,  0,    0,    0, false, false },
  /* HAPPY     */{ 30, 20,  0,  0,  0, 0.00f, EYE_ARC,    MOUTH_SMILE, false,  0,    0,    0, false, false },
  /* EXCITED   */{ 28, 38,  7,  9, 13, 0.00f, EYE_RECT,   MOUTH_GRIN,  false,  0,    0,    0, false, false },
  /* LOVE      */{ 30, 30,  0,  0,  0, 0.00f, EYE_HEART,  MOUTH_SMILE, false,  0,    0,    0, false, true  },
  /* COOL      */{ 30, 30,  0,  0,  0, 0.00f, EYE_SHADES, MOUTH_SMIRK, false,  0,    0,    0, false, false },
  /* WINK      */{ 30, 30, 12, 15,  9, 0.00f, EYE_RECT,   MOUTH_SMIRK, false,  0,    0,    0, true,  false },
  /* TIRED     */{ 30, 24, 12, 11,  8, 0.42f, EYE_RECT,   MOUTH_NONE,  false,  0,    0,    0, false, false },
  /* SAD       */{ 30, 28, 11, 12, 10, 0.15f, EYE_RECT,   MOUTH_FROWN, true,  -1,    0,    0, false, false },
  /* ANGRY     */{ 30, 28, 10, 10,  4, 0.20f, EYE_RECT,   MOUTH_FROWN, true,   1,    0,    0, false, false },
  /* SURPRISED */{ 26, 40,  9, 11, 13, 0.00f, EYE_RECT,   MOUTH_O,     false,  0,    0,    0, false, false },
  /* CONFUSED  */{ 30, 30, 11, 13,  9, 0.00f, EYE_RECT,   MOUTH_FLAT,  true,   0,    0,   -6, false, false },
  /* DEAD      */{ 28, 28,  0,  0,  0, 0.00f, EYE_CROSS,  MOUTH_FLAT,  false,  0,    0,    0, false, false },
};

static const char *kNames[FACE_COUNT] = {
  "neutral", "happy", "excited", "love",     "cool",       "wink",
  "tired",   "sad",   "angry",   "surprised", "confused",  "dead"
};

// Left / right eye centres.
static const int16_t kEyeY  = 33;
static const int16_t kEyeLX = 44;
static const int16_t kEyeRX = 84;

const char *faceName(FaceId f) { return (f < FACE_COUNT) ? kNames[f] : "neutral"; }

uint8_t faceCount() { return FACE_COUNT; }

const char *faceNameAt(uint8_t i) { return (i < FACE_COUNT) ? kNames[i] : ""; }

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
// Thick parabolic arc built from overlapping filled circles, so it works on any
// Adafruit_GFX version regardless of circle-helper quirks.
//   up = true  -> "∪" (a smile),  up = false -> "∩" (a frown / happy closed eye)
void FaceEngine::thickArc(Adafruit_GFX *g, int16_t x0, int16_t y0, int16_t w,
                          int16_t h, uint8_t thickness, bool up) {
  const int steps = w;
  const int r = thickness / 2;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;                           // 0..1
    int16_t x = x0 + (int16_t)(t * (w - 1));
    float curve = 1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f);  // 0..1..0
    int16_t y = up ? (int16_t)(y0 + h - curve * h)
                   : (int16_t)(y0 + curve * h);
    g->fillCircle(x, y, r, GFX_ON);
  }
}

void FaceEngine::drawArcEye(int16_t cx, int16_t cy, const FaceSpec &s) {
  thickArc(_gfx, cx - s.eyeW / 2, cy - s.eyeH / 2, s.eyeW, s.eyeH, 5, false);
}

void FaceEngine::drawRectEye(int16_t cx, int16_t cy, const FaceSpec &s, float lid) {
  const int16_t w = s.eyeW, h = s.eyeH;
  const int16_t x = cx - w / 2, y = cy - h / 2;
  const int16_t lidH = (int16_t)(h * lid);

  if (h - lidH <= 5) {                       // fully closed -> a thick line
    _gfx->fillRect(x, cy - 2, w, 4, GFX_ON);
    return;
  }

  _gfx->fillRoundRect(x, y, w, h, s.radius, GFX_ON);

  if (lidH > 0)                              // droopy eyelid
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
                      s.pupilH, min((int16_t)4, (int16_t)(s.pupilH / 2)), GFX_OFF);
}

// Two lobes plus a triangle - reads clearly as a heart even at 30 px.
void FaceEngine::drawHeartEye(int16_t cx, int16_t cy, int16_t size) {
  const int16_t r  = size / 4;
  const int16_t ly = cy - size / 6;
  _gfx->fillCircle(cx - r + 1, ly, r, GFX_ON);
  _gfx->fillCircle(cx + r - 1, ly, r, GFX_ON);
  _gfx->fillTriangle(cx - 2 * r + 1, ly + 1,
                     cx + 2 * r - 1, ly + 1,
                     cx, cy + size / 2, GFX_ON);
}

void FaceEngine::drawCrossEye(int16_t cx, int16_t cy, int16_t size) {
  const int16_t h = size / 2;
  for (int8_t t = -1; t <= 1; t++) {         // 3 px thick strokes
    _gfx->drawLine(cx - h, cy - h + t, cx + h, cy + h + t, GFX_ON);
    _gfx->drawLine(cx - h, cy + h + t, cx + h, cy - h + t, GFX_ON);
  }
}

void FaceEngine::drawShades(const FaceSpec &s) {
  const int16_t h = s.eyeH;
  const int16_t y = kEyeY - h / 2;
  const int16_t lx = kEyeLX - s.eyeW / 2;
  const int16_t rx = kEyeRX + s.eyeW / 2;

  _gfx->fillRoundRect(lx, y, s.eyeW, h, 6, GFX_ON);                  // left lens
  _gfx->fillRoundRect(kEyeRX - s.eyeW / 2, y, s.eyeW, h, 6, GFX_ON); // right lens
  _gfx->fillRect(lx + s.eyeW - 1, kEyeY - 3,                          // bridge
                 (kEyeRX - kEyeLX) - s.eyeW + 2, 6, GFX_ON);
  _gfx->fillRect(lx - 9, kEyeY - 4, 9, 4, GFX_ON);                    // temples
  _gfx->fillRect(rx, kEyeY - 4, 9, 4, GFX_ON);

  // A glossy slit so it reads as sunglasses rather than a black bar.
  _gfx->fillRect(lx + 4, y + 4, s.eyeW - 13, 3, GFX_OFF);
  _gfx->fillRect(kEyeRX - s.eyeW / 2 + 4, y + 4, s.eyeW - 13, 3, GFX_OFF);
}

void FaceEngine::drawBrow(int16_t cx, int16_t cy, const FaceSpec &s, bool left) {
  const int16_t half = s.eyeW / 2;
  const int8_t  lift = left ? s.browLiftL : s.browLiftR;
  const int16_t yTop = cy - s.eyeH / 2 - 6 + lift;
  const int16_t innerX = left ? cx + half - 3 : cx - half + 3;
  const int16_t outerX = left ? cx - half + 1 : cx + half - 1;
  const int16_t innerY = yTop + s.browSlant * 5;

  for (int8_t t = -1; t <= 1; t++)
    _gfx->drawLine(outerX, yTop + t, innerX, innerY + t, GFX_ON);
}

void FaceEngine::drawMouth(const FaceSpec &s) {
  switch (s.mouth) {
    case MOUTH_SMILE: thickArc(_gfx, 52, 48, 24, 10, 3, true);  break;
    case MOUTH_SMIRK: thickArc(_gfx, 60, 50, 18,  7, 3, true);  break;
    case MOUTH_FROWN: thickArc(_gfx, 52, 50, 24, 10, 3, false); break;
    case MOUTH_O:
      _gfx->drawCircle(64, 54, 5, GFX_ON);
      _gfx->drawCircle(64, 54, 4, GFX_ON);
      break;
    case MOUTH_FLAT:  _gfx->fillRect(56, 52, 16, 3, GFX_ON);    break;
    case MOUTH_GRIN:
      _gfx->fillRoundRect(48, 52, 32, 11, 5, GFX_ON);
      _gfx->fillRect(52, 55, 24, 3, GFX_OFF);                   // tooth line
      break;
    default: break;
  }
}

// Cheek marks, kept clear of the eyes (which span x 29..59 and 69..99).
void FaceEngine::drawBlush() {
  for (int8_t i = 0; i < 3; i++) {
    const int16_t x = 15 + i * 5;
    _gfx->drawLine(x, 45, x + 2, 51, GFX_ON);
    _gfx->drawLine(112 - i * 5, 45, 110 - i * 5, 51, GFX_ON);
  }
}

// ---------------------------------------------------------------------------
//  Frame
// ---------------------------------------------------------------------------
void FaceEngine::drawEyeAt(int16_t cx, int16_t cy, const FaceSpec &s, float lid,
                           bool forceArc) {
  switch (s.eye) {
    case EYE_ARC:
      drawArcEye(cx, cy, s);
      break;
    case EYE_HEART:
      drawHeartEye(cx, cy, s.eyeH);
      break;
    case EYE_CROSS:
      drawCrossEye(cx, cy, s.eyeH);
      break;

    case EYE_RECT:
    default:
      if (forceArc) {
        drawArcEye(cx, cy, s);
      } else if (_blinkAmt > 0.5f) {
        _gfx->fillRect(cx - s.eyeW / 2, cy - 2, s.eyeW, 4, GFX_ON);
      } else {
        drawRectEye(cx, cy, s, lid);
      }
      break;
  }
}

void FaceEngine::render() {
  if (!_gfx) return;

  const FaceSpec &s = kSpecs[_face];
  const float lid = s.baseLid + (1.0f - s.baseLid) * _blinkAmt;

  if (s.eye == EYE_SHADES) {
    drawShades(s);
  } else {
    drawEyeAt(kEyeLX, kEyeY, s, lid, s.winkLeft);
    drawEyeAt(kEyeRX, kEyeY, s, lid, false);
  }

  if (s.brows) {
    drawBrow(kEyeLX, kEyeY, s, true);
    drawBrow(kEyeRX, kEyeY, s, false);
  }

  drawMouth(s);

  if (s.blush) drawBlush();
}
