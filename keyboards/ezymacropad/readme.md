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
multiplexer selects which Hall sensor is sampled, and GP31 reads the selected
state back. The matrix scanner must therefore be implemented in a custom QMK
matrix driver, not by a simple direct-pin matrix.

## Build target

Once the custom matrix source is implemented for the mux, the board can be built
with:

```sh
qmk lint -kb ezymacropad
qmk compile -kb ezymacropad -km default
qmk flash -kb ezymacropad -km default
```
