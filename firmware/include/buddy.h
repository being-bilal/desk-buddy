// ---------------------------------------------------------------------------
//  Buddy - the whole device, minus the web layer.
//  Owns the display, the face engine, the text screen, audio, touch and the
//  persisted preferences. The web UI drives this class and reads its state.
// ---------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "config.h"

#include "audio.h"
#include "faces.h"
#include "text.h"
#include "touch.h"

enum ScreenMode : uint8_t {
  SCREEN_FACE = 0,
  SCREEN_TEXT
};

class Buddy {
public:
  void begin();
  void loop();

  // ------------------------- commands -------------------------------------
  void setFaceIndex(uint8_t i);
  void nextFace();
  bool setFaceByName(const char *name);

  void setText(const char *utf8);              // sanitise + persist
  void showTextNow(uint32_t holdMs = 0);       // what a long press does
  void hideText();

  void playIntro();
  void playTrack(uint8_t n);
  void stopAudio();
  void setVolume(uint8_t v);
  void cycleVolume();

  void pulseVibrate(uint16_t ms);
  void saveWifi(const char *ssid, const char *pass);
  bool wifiCredentials(char *ssid, size_t ssidLen, char *pass,
                       size_t passLen) const;

  // --------------------------- state --------------------------------------
  FaceId      face() const { return _faces.face(); }
  ScreenMode  mode() const { return _mode; }
  const char *text() const { return _text.raw(); }
  uint8_t     volume() const { return _audio.volume(); }
  bool        audioReady() const { return _audio.available(); }
  bool        playing() const { return _audio.playing(); }
  uint8_t     currentTrack() const { return _audio.currentTrack(); }
  bool        touched() const { return _touch.pressed(); }
  float       fps() const { return _fps; }
  uint32_t    uptimeMs() const { return millis(); }
  bool        storageReady() const { return _fsReady; }

  // Called by the web layer so it can push state to browsers.
  void (*onStateChanged)() = nullptr;

private:
  void loadPrefs();
  void savePrefs();
  void enterTextScreen(uint32_t holdMs);
  void splash();

  Adafruit_SSD1306 _display{OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET};
  FaceEngine       _faces;
  TextScreen       _text;
  Audio            _audio;
  Touch            _touch;

  ScreenMode _mode = SCREEN_FACE;
  uint32_t   _textShownAt = 0;
  uint32_t   _textHoldMs = 0;

  bool     _introPending = false;
  uint32_t _introDueAt = 0;

  float    _fps = 0;
  uint32_t _frameCount = 0;
  uint32_t _fpsWindowAt = 0;
  uint32_t _lastFrameAt = 0;

  bool     _fsReady = false;
  bool     _prefsDirty = false;
  uint32_t _prefsDirtyAt = 0;

  uint8_t  _faceIndex = 0;
  uint8_t  _audioVolume = AUDIO_VOLUME_DEFAULT;

  char _wifiSsid[33] = {0};
  char _wifiPass[65] = {0};
};
