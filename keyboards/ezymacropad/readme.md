# Ezymacropad

RP2040 Hall-effect macropad starter definition.

This board is an RP Pico-based Hall-effect macropad with a 4-to-1 mux select bus
and a single analog return line. There are no LEDs, no rotary encoders, and no
other peripherals fitted.

## Pin mapping

* GP2 = MUX Select 2
* GP3 = MUX Select 3
* GP4 = MUX Select 1
* GP5 = MUX Select 0
* GP36 = Power line, driven high to 3V3 when the mux is active
* GP31 = MUX output / analog input returning the selected Hall state

## Files in this directory

* `keyboard.json`: board identity and virtual key layout.
* `config.h`: compile-time board pin mapping.
* `board.h`: board-level GPIO definitions for the multiplexer bus.
* `rules.mk`: custom matrix driver selection.
* `keymaps/<name>/keymap.c`: user layers and key behavior.

## Hardware note

This is a custom matrix design rather than a direct GPIO-per-switch layout. The
multiplexer selects which Hall sensor is sampled, and GP26 reads the selected
state back. The matrix scanner must therefore be implemented in a custom QMK
matrix driver, not by a simple direct-pin matrix.

## MIDI mode

The board also shows up as a class-compliant USB MIDI device, next to the
normal keyboard. No driver is needed, so Ableton Live, FL Studio, Logic,
Reaper, Bitwig, GarageBand and friends pick it up as a MIDI input (named
"Ezymacropad") as soon as it is plugged in.

In the default keymap, hold the bottom-left key (MO(1)) and tap the second key
of the top row to switch to the MIDI layer. The bottom-left key (TO(0)) goes
back to the normal layer.

```
G#   A    A#   B
E    F    F#   G
C    C#   D    D#
back oct- oct+ vel
```

Notes start at C3 (MIDI note 48). `vel` (EZ_VTOG) switches between velocity
from key speed and a fixed velocity (127 unless changed with MI_VELU/MI_VELD).

Velocity comes from how fast each key travels from rest to actuation, timed by
the Hall sensors. If it feels too soft or too loud, tune
`EZY_MIDI_VELOCITY_FAST_US` and `EZY_MIDI_VELOCITY_SLOW_US` in `config.h`.
Uncommenting `EZY_MIDI_VELOCITY_DEBUG` and building with
`-e CONSOLE_ENABLE=yes` prints each key press's travel time and velocity to
`qmk console`. Set `MIDI_ENABLE = no` in `rules.mk` to remove MIDI completely.

## Build target

Once the custom matrix source is implemented for the mux, the board can be built
with:

```sh
qmk lint -kb ezymacropad
qmk compile -kb ezymacropad -km default
qmk flash -kb ezymacropad -km default
```
