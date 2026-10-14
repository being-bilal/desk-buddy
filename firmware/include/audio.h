// ---------------------------------------------------------------------------
//  Audio - two short melodies, played on a piezo disc via LEDC PWM by default,
//  or on a DFPlayer Mini over UART (config.h -> AUDIO_DRIVER_PIEZO == 0).
//
//     track 1 -> intro jingle, played once at boot
//     track 2 -> plays together with the long-press text screen
//
//  Playback is a non-blocking step sequencer driven from update(), so the face
//  animation keeps running while a melody plays. A piezo can only make tones -
//  it cannot play MP3s or speech.
// ---------------------------------------------------------------------------
#pragma once

#include <stdint.h>

struct Note {
  uint16_t freq;   // Hz, 0 = rest
  uint16_t ms;     // duration
};

class Audio {
public:
  bool begin();                       // false if the chosen driver failed
  void update(uint32_t nowMs);

  void playIntro();
  void playMessage();
  void playTrack(uint8_t n);
  void stop();

  void    setVolume(uint8_t v);       // 0..30, 0 = mute
  uint8_t volume() const { return _vol; }
  void    cycleVolume();

  bool    available() const { return _ready; }
  bool    playing() const { return _playing; }
  uint8_t currentTrack() const { return _track; }

private:
  void start(const Note *melody, uint8_t track);
  void tone(uint16_t freqHz);
  void silence();
  void applyVolume();

  bool    _ready = false;
  bool    _playing = false;
  uint8_t _vol = 20;
  uint8_t _track = 0;

  // ---- piezo sequencer ----
  const Note *_melody = nullptr;
  uint8_t     _step = 0;
  uint32_t    _stepAt = 0;
  uint16_t    _freq = 0;

  // ---- DFPlayer bookkeeping ----
  uint32_t _startedAt = 0;
  uint32_t _estimateMs = 0;
};
