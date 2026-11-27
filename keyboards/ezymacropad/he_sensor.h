#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "he_adc_dma.h"

/* Press when |reading - baseline| exceeds the threshold; release once it
 * falls below (threshold - hysteresis). Values are in 10-bit ADC counts. */
#ifndef MUX_ANALOG_THRESHOLD
#    define MUX_ANALOG_THRESHOLD 35
#endif
#ifndef MUX_ANALOG_HYSTERESIS
#    define MUX_ANALOG_HYSTERESIS 12
#endif

/* Frames averaged into each key's resting baseline at start-up. Keys are
 * reported released until this completes (~HE_BASELINE_FRAMES * frame period). */
#ifndef HE_BASELINE_FRAMES
#    define HE_BASELINE_FRAMES 64
#endif

/* Travel timing, used for MIDI note velocity. For every key the time from
 * leaving rest (|delta| above HE_TRAVEL_START_THRESHOLD) to actuation (above
 * MUX_ANALOG_THRESHOLD) is measured; a faster stroke means a shorter time.
 * Only built when MIDI is enabled. Threshold is in 10-bit ADC counts like the
 * others, and must sit above the sensor noise and below the release point. */
#if defined(MIDI_ENABLE) && !defined(HE_TRAVEL_TIMING_DISABLE)
#    define HE_TRAVEL_TIMING
#endif
#ifndef HE_TRAVEL_START_THRESHOLD
#    define HE_TRAVEL_START_THRESHOLD 12
#endif

void he_sensor_init(void);

/* Processes one frame. Returns the pressed state as a bitmap indexed by mux
 * address. Keys whose samples carry an ADC error keep their previous state. */
uint16_t he_sensor_process(const he_frame_t *frame);

bool     he_sensor_is_calibrated(void);
uint32_t he_sensor_sample_errors(void);

#ifdef HE_TRAVEL_TIMING
/* Microseconds the key at this mux address took from leaving rest to its most
 * recent actuation. 0 means it crossed both levels within one frame (as fast
 * as can be measured). Valid from the frame that reported the press. */
uint32_t he_sensor_travel_us(uint8_t address);
#endif
