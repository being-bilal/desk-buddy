#include "buddy.h"
#include "config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

// ---------------------------------------------------------------------------
//  Boot
// ---------------------------------------------------------------------------
void Buddy::begin() {
  Serial.begin(115200);
  delay(150);
  Serial.printf("\n=== %s v%s ===\n", FW_NAME, FW_VERSION);

  // ------------------------------ display --------------------------------
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[display] no SSD1306 at 0x3C - check SDA/SCL and the "
                   "3V3 rail");
  }
  _display.clearDisplay();
  _display.display();
  _faces.begin(&_display);

  // ------------------------------ storage --------------------------------
  _fsReady = LittleFS.begin(/*formatOnFail=*/true);
  if (!_fsReady) Serial.println("[fs] LittleFS mount failed");

  loadPrefs();          // may fill _wifiSsid/_wifiPass, face and volume

  // Restore the saved message.
  if (_fsReady && LittleFS.exists(TEXT_PATH)) {
    File f = LittleFS.open(TEXT_PATH, "r");
    if (f) {
      _text.setText(f.readString().c_str());
      f.close();
    }
  }

  // ------------------------------- input ---------------------------------
  _touch.begin();

  // ------------------------------- audio ---------------------------------
  _audio.begin();
  _audio.setVolume(_audioVolume ? _audioVolume : AUDIO_VOLUME_DEFAULT);

  _faces.setFace((FaceId)_faceIndex);

  splash();

  // Let the display, web server and WiFi settle before the first chirp.
  _introPending = true;
  _introDueAt = millis() + 700;

  _lastFrameAt = _fpsWindowAt = millis();
}

// ---------------------------------------------------------------------------
//  Main loop
// ---------------------------------------------------------------------------
void Buddy::loop() {
  const uint32_t now = millis();

  // --- gestures: polled every iteration so short taps are never missed ----
  switch (_touch.poll(now)) {
    case TOUCH_TAP:
      if (_mode == SCREEN_TEXT) {
        hideText();
      } else {
        nextFace();
      }
      pulseVibrate(15);
      break;

    case TOUCH_LONG_PRESS:
      pulseVibrate(45);
      enterTextScreen(0);
      break;

    default:
      break;
  }

  _audio.update(now);

  // --- deferred intro jingle ---------------------------------------------
  if (_introPending && (int32_t)(now - _introDueAt) >= 0) {
    _introPending = false;
    playIntro();
  }

  // --- text screen holds at least as long as its track -------------------
  if (_mode == SCREEN_TEXT) {
    const uint32_t elapsed = now - _textShownAt;
    bool keep = (elapsed < _textHoldMs);
    // Keep it up while the message track is still playing, with a hard cap so
    // a missing BUSY line can never strand the face.
    if (!keep && _audio.playing() && elapsed < 30000) keep = true;
    if (!keep) hideText();
  }

  // --- animation ---------------------------------------------------------
  if ((uint32_t)(now - _lastFrameAt) >= FRAME_MS) {
    _lastFrameAt = now;

    _faces.update(now);

    _display.clearDisplay();
    if (_mode == SCREEN_TEXT) {
      _text.render(&_display, now - _textShownAt);
    } else {
      _faces.render();               // framebuffer already cleared above
    }
    _display.display();

    _frameCount++;
    if ((uint32_t)(now - _fpsWindowAt) >= 1000) {
      _fps = (float)_frameCount * 1000.0f / (float)(now - _fpsWindowAt);
      _frameCount = 0;
      _fpsWindowAt = now;
    }
  }

  // --- debounced preference flush ----------------------------------------
  if (_prefsDirty && (uint32_t)(now - _prefsDirtyAt) > 800) {
    _prefsDirty = false;
    savePrefs();
  }
}

// ---------------------------------------------------------------------------
//  Commands
// ---------------------------------------------------------------------------
void Buddy::setFaceIndex(uint8_t i) {
  if (i >= FACE_COUNT) return;
  _faces.setFace((FaceId)i);
  if (_mode == SCREEN_TEXT) hideText();
  _faceIndex = i;
  _prefsDirty = true;
  _prefsDirtyAt = millis();
  if (onStateChanged) onStateChanged();
}

void Buddy::nextFace() {
  _faces.nextFace();
  _faceIndex = (uint8_t)_faces.face();
  _prefsDirty = true;
  _prefsDirtyAt = millis();
  if (onStateChanged) onStateChanged();
}

bool Buddy::setFaceByName(const char *name) {
  const int i = faceFromName(name);
  if (i < 0) return false;
  setFaceIndex((uint8_t)i);
  return true;
}

void Buddy::setText(const char *utf8) {
  _text.setText(utf8);
  if (_fsReady) {
    File f = LittleFS.open(TEXT_PATH, "w");
    if (f) {
      f.print(_text.raw());
      f.close();
    }
  }
  if (onStateChanged) onStateChanged();
}

void Buddy::enterTextScreen(uint32_t holdMs) {
  // Nothing to show: still play the message track so a long press always
  // feels like it did something.
  if (_text.empty()) {
    playTrack(TRACK_LONGPRESS);
    return;
  }
  _mode = SCREEN_TEXT;
  _textShownAt = millis();
  _textHoldMs = holdMs ? holdMs : _text.durationMs();
  playTrack(TRACK_LONGPRESS);
  if (onStateChanged) onStateChanged();
}

void Buddy::showTextNow(uint32_t holdMs) { enterTextScreen(holdMs); }

void Buddy::hideText() {
  if (_mode != SCREEN_TEXT) return;
  _mode = SCREEN_FACE;
  if (onStateChanged) onStateChanged();
}

void Buddy::playIntro()   { _audio.playIntro();   if (onStateChanged) onStateChanged(); }
void Buddy::playTrack(uint8_t n) { _audio.playTrack(n); if (onStateChanged) onStateChanged(); }
void Buddy::stopAudio()   { _audio.stop();        if (onStateChanged) onStateChanged(); }
void Buddy::setVolume(uint8_t v) {
  _audio.setVolume(v);
  _audioVolume = _audio.volume();
  _prefsDirty = true;
  _prefsDirtyAt = millis();
  if (onStateChanged) onStateChanged();
}
void Buddy::cycleVolume() { setVolume(_audio.volume() >= 30 ? 0 : _audio.volume() + 5); }

void Buddy::pulseVibrate(uint16_t ms) {
#if PIN_VIBRO >= 0
  digitalWrite(PIN_VIBRO, HIGH);
  delay(ms);
  digitalWrite(PIN_VIBRO, LOW);
#else
  (void)ms;
#endif
}

void Buddy::saveWifi(const char *ssid, const char *pass) {
  strncpy(_wifiSsid, ssid ? ssid : "", sizeof(_wifiSsid) - 1);
  strncpy(_wifiPass, pass ? pass : "", sizeof(_wifiPass) - 1);
  _wifiSsid[sizeof(_wifiSsid) - 1] = '\0';
  _wifiPass[sizeof(_wifiPass) - 1] = '\0';
  savePrefs();
}

bool Buddy::wifiCredentials(char *ssid, size_t ssidLen, char *pass,
                            size_t passLen) const {
  if (!_wifiSsid[0]) return false;
  strncpy(ssid, _wifiSsid, ssidLen - 1);
  ssid[ssidLen - 1] = '\0';
  strncpy(pass, _wifiPass, passLen - 1);
  pass[passLen - 1] = '\0';
  return true;
}

// ---------------------------------------------------------------------------
//  Persistence
// ---------------------------------------------------------------------------
void Buddy::loadPrefs() {
  if (!_fsReady || !LittleFS.exists(PREFS_PATH)) return;
  File f = LittleFS.open(PREFS_PATH, "r");
  if (!f) return;

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[prefs] parse failed: %s\n", err.c_str());
    return;
  }

  _faceIndex   = doc["face"] | 0;
  _audioVolume = doc["vol"] | AUDIO_VOLUME_DEFAULT;
  if (_faceIndex >= FACE_COUNT) _faceIndex = 0;

  const char *ssid = doc["ssid"] | "";
  const char *pass = doc["pass"] | "";
  strncpy(_wifiSsid, ssid, sizeof(_wifiSsid) - 1);
  strncpy(_wifiPass, pass, sizeof(_wifiPass) - 1);
}

void Buddy::savePrefs() {
  if (!_fsReady) return;
  JsonDocument doc;
  doc["face"] = _faceIndex;
  doc["vol"]  = _audioVolume;
  doc["ssid"] = _wifiSsid;
  doc["pass"] = _wifiPass;

  File f = LittleFS.open(PREFS_PATH, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

// ---------------------------------------------------------------------------
void Buddy::splash() {
  _display.clearDisplay();
  _display.setTextColor(1);
  _display.setTextSize(1);
  _display.setCursor(14, 20);
  _display.print(FW_NAME);
  _display.setCursor(34, 36);
  _display.printf("v%s", FW_VERSION);
  _display.display();
  delay(900);
}
