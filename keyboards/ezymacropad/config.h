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
