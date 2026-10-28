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

void he_sensor_init(void);

/* Processes one frame. Returns the pressed state as a bitmap indexed by mux
 * address. Keys whose samples carry an ADC error keep their previous state. */
uint16_t he_sensor_process(const he_frame_t *frame);

bool     he_sensor_is_calibrated(void);
uint32_t he_sensor_sample_errors(void);
