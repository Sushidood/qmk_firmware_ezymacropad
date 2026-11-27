#pragma once

#define DYNAMIC_KEYMAP_LAYER_COUNT 4

/* RP Pico-based Hall-effect multiplexer board.
 * No LEDs, rotary encoders, or other peripherals are fitted.
 */
#define MUX_SELECT_0_PIN GP5
#define MUX_SELECT_1_PIN GP4
#define MUX_SELECT_2_PIN GP2
#define MUX_SELECT_3_PIN GP3

#define MUX_ADC_INPUT_PIN GP26

/* Mux settling is handled by discarding the first samples of every channel
 * (2 us each); see he_adc_dma.h. */
#define HE_ADC_DISCARD_SAMPLES 2
#define HE_ADC_OVERSAMPLE 4

/* Analog hysteresis in he_sensor.c replaces mechanical-switch debounce, which
 * would only add latency. Raise this if keys chatter. */
#define DEBOUNCE 0

/* Uncomment (with CONSOLE_ENABLE = yes in rules.mk) to print acquisition
 * statistics to `qmk console` once a second. */
// #define HE_STATS_ENABLE

/* Uncomment to output ISR activity / frame toggles for a scope or logic analyzer. */
// #define HE_DEBUG_ISR_PIN GP6
// #define HE_DEBUG_FRAME_PIN GP7

/* MIDI mode (MIDI_ENABLE in rules.mk). The board shows up as a class-compliant
 * USB MIDI device next to the keyboard, so DAWs see it without drivers.
 * MIDI_ADVANCED gives octave, transpose, velocity and channel keycodes. */
#define MIDI_ADVANCED

/* Note velocity from how fast a key is struck, measured over the first part of
 * its travel (from HE_TRAVEL_START_THRESHOLD to MUX_ANALOG_THRESHOLD, see
 * he_sensor.h). A stroke this fast or faster plays EZY_MIDI_VELOCITY_MAX; this
 * slow or slower plays EZY_MIDI_VELOCITY_MIN. Tune them to your playing with
 * EZY_MIDI_VELOCITY_DEBUG, which prints each stroke's time to `qmk console`.
 * EZ_VTOG switches between sensed and fixed velocity at run time. */
#define EZY_MIDI_VELOCITY_FAST_US 2000
#define EZY_MIDI_VELOCITY_SLOW_US 40000
#define EZY_MIDI_VELOCITY_MAX 127
#define EZY_MIDI_VELOCITY_MIN 16
// #define EZY_MIDI_VELOCITY_DEBUG
