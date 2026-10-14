# Wiring

Everything runs on 3.3 V logic. The ESP32-C3 Super Mini's **native USB** means
the browser flasher works with no driver, but it also means GPIO18/19 are not
free.

## Pin map

| ESP32-C3 | goes to | notes |
|---|---|---|
| `3V3` | OLED `VCC` | 0.96" SSD1306, I2C address `0x3C` |
| `GND` | OLED `GND` | |
| `GPIO4` | OLED `SDA` | I2C data |
| `GPIO5` | OLED `SCL` | I2C clock |
| `3V3` | TTP223 `VCC` | |
| `GND` | TTP223 `GND` | |
| `GPIO3` | TTP223 `OUT` / `I/O` | active HIGH |
| `GPIO2` | piezo `+` via a 100 Ω resistor | **strapping pin — read below** |
| `GND` | piezo `−` | |

No DAC is used and none exists on the C3: the piezo is driven by **LEDC PWM**,
so only one GPIO is needed and no support components beyond the series resistor.

`GPIO8` drives the on-board LED (active LOW) and `GPIO9` is the BOOT button.

## GPIO2 is a strapping pin

The C3 samples three pins at **every** reset:

| strapping pin | SPI Boot (normal) | Download Boot | internal pull-up |
|---|---|---|---|
| **`GPIO2`** | **1** | **1** | **none** |
| `GPIO8` | irrelevant | 1 | none |
| `GPIO9` | 1 | 0 | weak |

GPIO2 must be **high at every reset**, and unlike GPIO9 it has no internal
pull-up. So:

- **A bare piezo disc is fine.** It is capacitive — roughly 10–20 nF with a very
  high DC resistance — so it presents no DC load and cannot hold the pin low.
- **A buzzer *module* may not be.** Active buzzer boards and transistor driver
  modules often fit a pulldown resistor on the input. That will hold GPIO2 low
  at reset, and the board will either refuse to boot or fall into an unexpected
  mode. This is the number one way to break a C3 with a buzzer.

If the board becomes unreliable at power-on, do one of these:

1. Move the buzzer to a free non-strapping pin — `GPIO6`, `GPIO7`, `GPIO10`,
   `GPIO20` or `GPIO21` — and change `PIN_BUZZER` in `include/config.h`.
2. Fit a **10 kΩ pull-up from GPIO2 to 3V3** and keep the buzzer where it is.

Also avoid `GPIO2`, `GPIO8` and `GPIO9` for anything that can pull the pin low
at boot: buttons to ground, I2C pull-downs, or long unshielded runs.

## Passive vs active buzzer

This matters more than the wiring:

| Part | Behaviour | Use it? |
|---|---|---|
| **Passive piezo disc** (2 bare wires, no board) | Plays any frequency you feed it | **Yes — this is what the firmware assumes** |
| Active buzzer (sealed, has its own oscillator) | Emits one fixed tone, on or off only | No — you cannot play a melody |
| Buzzer module (small PCB with a transistor) | Usually active, plus a pulldown that fights GPIO2 | No |

If you have an active buzzer, you will get a single flat beep instead of the
jingle, and no volume control.

## Volume, and what the piezo can't do

A piezo is loudest when driven at **50 % duty**, so volume maps 0–30 onto duty
0–512 in the firmware's 10-bit LEDC mode. Volume 0 is genuine silence.

Be realistic about the part: a piezo plays **tones**. It cannot play MP3s,
music or speech. The two sounds are short melodies defined in
`firmware/src/audio.cpp` — a four-note rising jingle for boot, and a three-note
descending chime for the message screen. Edit the `kIntro` and `kMessage`
arrays to change them:

```cpp
static const Note kIntro[] = {
  {523,  80}, {659,  80}, {784,  80}, {1047, 200}, {0, 60}, {0, 0}
};
//  ^Hz  ^ms   ...            0 Hz = rest, {0,0} terminates
```

Useful frequencies: `C5=523 D5=587 E5=659 G5=784 A5=880 C6=1047`.

If you later want real audio — music, voice, longer clips — the firmware has a
DFPlayer Mini path already written: set `AUDIO_DRIVER_PIEZO` to `0` in
`include/config.h`, wire the module per the comments there, and remember the
**1 kΩ series resistor on its RX line** plus a **100–470 µF capacitor across its
5 V**. The `Audio` class hides the difference, so no other file changes.

## Check your board's silkscreen

ESP32-C3 Super Mini clones vary. Some expose different GPIO numbers on the
castellated pads, and a few omit `GPIO6`/`GPIO7` entirely. Before soldering,
confirm the pads labelled around the board and adjust the pin defines at the top
of `firmware/include/config.h` if your layout differs. The firmware has no
hard-coded pin assumptions beyond that file.

## Touch module

TTP223 modules ship in **momentary, active HIGH** mode with pads on the back for
other modes. If yours is latched/toggle or active LOW:

- move the jumper off the `A` pad (toggle) if it's set, and
- flip `TOUCH_ACTIVE_HIGH` to `0` in `include/config.h`.

The firmware assumes a plain digital input: the ESP32-C3 **has no on-chip
capacitive-touch peripheral**, so there is nothing to tune — the gesture logic
(tap vs. 0.7 s hold) runs in software on top of that pin.

## Optional extras

| Define | Default | Purpose |
|---|---|---|
| `PIN_VIBRO` | `-1` | Vibration motor module; pulses on every tap. |
| `PIN_DF_BUSY` | `-1` | DFPlayer only. `LOW` while playing, for an honest "playing" flag. |
