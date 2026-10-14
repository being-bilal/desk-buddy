# Desk Buddy

A desk companion on an **ESP32-C3 Super Mini**: an animated OLED face, a
capacitive touch pad, and a piezo buzzer for sound. It is a from-scratch take
on the [leroyd.com Desk Companion](https://leroyd.com/) — same hardware
family, but the message and audio behaviour is driven from a web panel the
device serves itself.

```
single tap   ->  cycle through 5 faces
hold ~0.7 s  ->  show the message stored from the web panel + melody 2
power on     ->  chirp melody 1 (the intro jingle)
```

## What's here

```
desk-buddy/
├── firmware/
│   ├── platformio.ini          ESP32-C3 Super Mini, 4 MB, dual-OTA + LittleFS
│   ├── partitions.csv          2 x 1.81 MB app slots + 320 KB LittleFS
│   ├── include/                config.h (pins), faces.h, text.h, audio.h,
│   │                           touch.h, buddy.h, webui.h
│   ├── src/                    face engine, text renderer, melody sequencer,
│   │                           touch gestures, app state, HTTP/WebSocket layer
│   └── data/                   the on-device web panel (served from LittleFS)
├── web-flasher/                HTTPS installer page + esp-web-tools manifest
├── tools/                      build_release.sh, merge_firmware.py
└── docs/
    ├── WIRING.md               pin map, SD card layout, gotchas
    └── BUILD.md                toolchain, flashing, installer hosting
```

## Quick start

```bash
cd desk-buddy
python3 -m venv .venv-pio && ./.venv-pio/bin/python -m pip install platformio esptool
./tools/build_release.sh
```

Flash over USB:

```bash
cd firmware
PLATFORMIO_CORE_DIR="$PWD/../.pio-core" ../.venv-pio/bin/pio run -t upload
PLATFORMIO_CORE_DIR="$PWD/../.pio-core" ../.venv-pio/bin/pio run -t uploadfs
```

Wire it up per [docs/WIRING.md](docs/WIRING.md), then read the console.

## First-run flow

1. No WiFi credentials stored → the buddy starts an access point called
   **`DeskBuddy-Setup`** with a captive portal.
2. Join it; the setup sheet opens (otherwise browse to `http://192.168.4.1`).
3. Enter your network details; the device saves them and reboots.
4. Open the control panel at **`http://buddy.local`** (or the IP printed on the
   serial console).

Credentials are stored in LittleFS, and if the network is later unreachable the
device falls back to the setup AP rather than becoming a brick.

## The faces

Five expressions, each with its own geometry, all animated with a wandering
gaze and randomised blinking:

| # | Face | Look |
|---|---|---|
| 0 | `neutral` | tall rounded eyes, mid pupils |
| 1 | `happy` | closed "∩ ∩" arc eyes and a smile |
| 2 | `tired` | eyelids covering ~40 % of the eye |
| 3 | `angry` | inward-slanting brows, narrowed pupils |
| 4 | `surprised` | oversized eyes, small pupils, "o" mouth |

The renderer draws only through `Adafruit_GFX` and uses no driver-specific
constants, so moving to an SH1106 or a colour panel means changing the display
object in `buddy.h`, not the face engine.

## Web panel protocol

The panel talks to the firmware over a WebSocket at `ws://<device>/ws`.

**Browser → device**

| Message | Effect |
|---|---|
| `{"t":"face","v":"happy"}` | select a face by name |
| `{"t":"face","i":2}` | select a face by index |
| `{"t":"next"}` | advance one face |
| `{"t":"text","v":"..."}` | store a message (persisted to LittleFS) |
| `{"t":"show"}` / `{"t":"show","ms":4000}` | display it now, like a long press |
| `{"t":"hide"}` | return to the face |
| `{"t":"play","n":1}` | play track `n` |
| `{"t":"intro"}` | replay the intro jingle |
| `{"t":"stop"}` | stop audio |
| `{"t":"vol","v":20}` | set volume, 0–30 |
| `{"t":"get"}` | request a state frame |

**Device → browser** — a single `state` frame, pushed on every change and on
connect:

```json
{"t":"state","fw":"0.1.0","name":"Desk Buddy","face":"happy","faceIndex":1,
 "faceCount":5,"mode":"face","text":"Hello","vol":20,"track":0,"playing":false,
 "audio":true,"storage":true,"fps":30.1,"uptime":42,"ap":false,"ip":"10.0.0.7"}
```

There is also a small REST surface: `GET /api/state`, `POST /api/text`
(plain-text body), `GET|POST /api/wifi`, `POST /api/reboot`.

## Design notes

**No capacitive-touch peripheral.** The ESP32-C3 doesn't have one, so a
TTP223-style module feeds a plain digital pin and the tap/hold distinction is
software timing. This is why the parts list includes a touch *module*.

**No DAC.** Also unlike the classic ESP32, which is why the piezo is driven by
LEDC PWM rather than an analogue output.

**A piezo makes tones, not music.** It cannot play MP3s or speech. The two
sounds are short melodies defined in `src/audio.cpp`. A DFPlayer Mini path is
already written behind `AUDIO_DRIVER_PIEZO 0` if you later want real audio.

**Audio needs no storage.** Because the melodies are a few dozen bytes of code,
the 320 KB LittleFS partition only has to hold the web panel — which in turn
makes dual-OTA slots affordable.

**ASCII-only font.** The stock Adafruit GFX font has no glyphs beyond ASCII,
so incoming text is folded (`é`→`e`) and emoji are dropped rather than rendered
as boxes. Embedding a custom GFX font is the upgrade path for real coverage.

## Possible next steps

- **OTA updates** — the `app1` partition is already there; add a
  `POST /api/update` endpoint that writes the spare slot and flips `otadata`.
- **A clock face** — push `{"t":"text"}` from the browser on a timer, or add
  SNTP and a sixth screen that renders `HH:MM`.
- **Longer messages** — the text screen already scrolls past 7 wrapped rows.
- **Improv WiFi** — set `"improv": true` in the flasher manifest and implement
  the serial protocol so users can configure WiFi during flashing.
- **Vibration** — wire a motor module and set `PIN_VIBRO` in `include/config.h`
  for haptic feedback on every tap.
- **Real audio** — set `AUDIO_DRIVER_PIEZO 0` and wire a DFPlayer Mini plus a
  speaker for music and voice. See [docs/WIRING.md](docs/WIRING.md).
