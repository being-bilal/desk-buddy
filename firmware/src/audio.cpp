#include "audio.h"
#include "config.h"

#include <Arduino.h>

#if AUDIO_DRIVER_PIEZO

// ---------------------------------------------------------------------------
//  Melodies. 0 Hz is a rest; a {0, 0} entry terminates the sequence.
//  Note frequencies: C5=523 D5=587 E5=659 G5=784 A5=880 C6=1047
// ---------------------------------------------------------------------------
static const Note kIntro[] = {
  {523,  80}, {659,  80}, {784,  80}, {1047, 200}, {0, 60}, {0, 0}
};

static const Note kMessage[] = {
  {784, 110}, {0, 25}, {659, 110}, {0, 25}, {523, 280}, {0, 0}
};

static const Note kTap[] = {
  {1047, 40}, {0, 0}
};

static const Note *melodyFor(uint8_t n) {
  switch (n) {
    case TRACK_LONGPRESS: return kMessage;
    case TRACK_TAP:       return kTap;
    case TRACK_INTRO:
    default:              return kIntro;
  }
}

#else   // ------------------------- DFPlayer Mini ---------------------------

#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>

// UART1 on the C3's GPIO matrix - any free pin works.
static HardwareSerial dfSerial(1);
static DFRobotDFPlayerMini df;

#endif  // AUDIO_DRIVER_PIEZO

// ---------------------------------------------------------------------------
//  Setup
// ---------------------------------------------------------------------------
bool Audio::begin() {
  _vol = AUDIO_VOLUME_DEFAULT;

#if AUDIO_DRIVER_PIEZO
  // Seed the timer, then hand the pin to the channel. Duty stays 0 until a
  // melody starts, so the buzzer is silent at boot.
  ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_IDLE_FREQ, BUZZER_LEDC_BITS);
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
  ledcWrite(BUZZER_LEDC_CHANNEL, 0);
  _ready = true;
  Serial.printf("[audio] piezo on GPIO%d (LEDC ch %d)\n", PIN_BUZZER,
                BUZZER_LEDC_CHANNEL);
#else
#if PIN_DF_BUSY >= 0
  pinMode(PIN_DF_BUSY, INPUT_PULLUP);
#endif
  dfSerial.begin(DFPLAYER_BAUD, SERIAL_8N1, PIN_DF_RX, PIN_DF_TX);
  delay(400);                           // the DFPlayer needs a moment
  _ready = df.begin(dfSerial, /*isACK=*/false, /*doReset=*/true);
  if (_ready) {
    df.volume(_vol);
    df.stop();
  } else {
    Serial.println("[audio] DFPlayer Mini not responding - check the 1k "
                   "resistor on its RX line and that its 5V rail is solid.");
  }
#endif

  return _ready;
}

// ---------------------------------------------------------------------------
//  Sequencer
// ---------------------------------------------------------------------------
void Audio::update(uint32_t now) {
#if AUDIO_DRIVER_PIEZO
  if (!_playing || !_melody) return;
  if ((uint32_t)(now - _stepAt) < _melody[_step].ms) return;

  _step++;
  const Note &n = _melody[_step];
  if (n.freq == 0 && n.ms == 0) {       // terminator
    silence();
    _playing = false;
    _track = 0;
    return;
  }
  _stepAt = now;
  tone(n.freq);
#else
  if (_playing && (uint32_t)(now - _startedAt) > _estimateMs)
    _playing = false;
#endif
}

#if AUDIO_DRIVER_PIEZO
void Audio::tone(uint16_t freq) {
  _freq = freq;
  if (freq == 0) {
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    return;
  }
  // ledcWriteTone sets the frequency and forces ~50% duty; ledcWrite then only
  // overrides the duty, leaving the frequency untouched.
  ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
  applyVolume();
}

void Audio::applyVolume() {
  if (!_playing || _freq == 0) {
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    return;
  }
  // A piezo is loudest at 50% duty (512 of the 1024 steps in 10-bit mode), so
  // "volume" is just how far towards 50% we drive it.
  const uint32_t half = 1u << (BUZZER_LEDC_BITS - 1);
  ledcWrite(BUZZER_LEDC_CHANNEL, (uint32_t)_vol * half / 30u);
}

void Audio::silence() {
  ledcWrite(BUZZER_LEDC_CHANNEL, 0);
  _freq = 0;
}

void Audio::start(const Note *melody, uint8_t track) {
  if (!_ready || !melody) return;
  _melody = melody;
  _step = 0;
  _track = track;
  _playing = true;
  _stepAt = millis();
  tone(melody[0].freq);
}
#endif  // AUDIO_DRIVER_PIEZO

// ---------------------------------------------------------------------------
//  Commands
// ---------------------------------------------------------------------------
void Audio::playTrack(uint8_t n) {
#if AUDIO_DRIVER_PIEZO
  start(melodyFor(n), n);
#else
  _track = n;
  _startedAt = millis();
  _estimateMs = (n == TRACK_INTRO) ? 6000 : 15000;
  _playing = true;
  if (!_ready) return;
#if DFPLAYER_USE_MP3_FOLDER
  df.playMp3Folder(n);              // /mp3/0001.mp3 ...
#else
  df.play(n);                       // /0001.mp3 ...
#endif
#endif
}

void Audio::playIntro()   { playTrack(TRACK_INTRO); }
void Audio::playMessage() { playTrack(TRACK_LONGPRESS); }

void Audio::stop() {
  _playing = false;
  _track = 0;
#if AUDIO_DRIVER_PIEZO
  silence();
#else
  _playing = false;
  if (_ready) df.stop();
#endif
}

void Audio::setVolume(uint8_t v) {
  _vol = (v > 30) ? 30 : v;
#if AUDIO_DRIVER_PIEZO
  applyVolume();
#else
  if (_ready) df.volume(_vol);
#endif
}

void Audio::cycleVolume() {
  static const uint8_t kSteps[] = {0, 5, 10, 15, 20, 25, 30};
  uint8_t next = kSteps[0];
  for (uint8_t i = 0; i < sizeof(kSteps); i++) {
    if (kSteps[i] > _vol) { next = kSteps[i]; break; }
    next = kSteps[0];               // wrapped back round to mute
  }
  setVolume(next);
}
