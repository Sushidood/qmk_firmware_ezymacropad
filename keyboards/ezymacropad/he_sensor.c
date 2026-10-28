#include "he_sensor.h"

_Static_assert(MUX_ANALOG_THRESHOLD > MUX_ANALOG_HYSTERESIS, "MUX_ANALOG_HYSTERESIS must be smaller than MUX_ANALOG_THRESHOLD");

/* The thresholds are in 10-bit counts (what analogReadPin returned when they
 * were tuned), but the DMA engine stores raw 12-bit samples, so scale by 4.
 * Everything below works on sums of HE_ADC_OVERSAMPLE samples, so the
 * thresholds are scaled once here instead of dividing every reading. */
#define HE_THRESHOLD_SCALE 4
#define PRESS_DELTA ((int32_t)MUX_ANALOG_THRESHOLD * HE_THRESHOLD_SCALE * HE_ADC_OVERSAMPLE)
#define RELEASE_DELTA ((int32_t)(MUX_ANALOG_THRESHOLD - MUX_ANALOG_HYSTERESIS) * HE_THRESHOLD_SCALE * HE_ADC_OVERSAMPLE)

/* All per-key arrays are indexed by mux address. */
static uint32_t baseline_acc[HE_KEY_COUNT];
static uint16_t baseline_count[HE_KEY_COUNT];
static int32_t  baseline[HE_KEY_COUNT];
static uint16_t baseline_frames;
static bool     calibrated;
static uint16_t pressed_mask;
static uint32_t sample_errors;

void he_sensor_init(void) {
    for (uint8_t i = 0; i < HE_KEY_COUNT; i++) {
        baseline_acc[i]   = 0;
        baseline_count[i] = 0;
        baseline[i]       = 0;
    }
    baseline_frames = 0;
    calibrated      = false;
    pressed_mask    = 0;
    sample_errors   = 0;
}

static void finish_baseline(void) {
    for (uint8_t i = 0; i < HE_KEY_COUNT; i++) {
        baseline[i] = baseline_count[i] ? (int32_t)(baseline_acc[i] / baseline_count[i]) : 0;
    }
    calibrated = true;
}

uint16_t he_sensor_process(const he_frame_t *frame) {
    for (uint8_t slot = 0; slot < HE_KEY_COUNT; slot++) {
        const uint8_t   address = he_scan_order[slot];
        const uint16_t *samples = &frame->samples[slot][HE_ADC_DISCARD_SAMPLES];

        uint32_t sum   = 0;
        uint16_t flags = 0;
        for (uint8_t i = 0; i < HE_ADC_OVERSAMPLE; i++) {
            sum += samples[i];
            flags |= samples[i];
        }
        if (flags & HE_ADC_SAMPLE_ERR) {
            sample_errors++;
            continue;
        }

        if (!calibrated) {
            baseline_acc[address] += sum;
            baseline_count[address]++;
            continue;
        }

        int32_t delta = (int32_t)sum - baseline[address];
        if (delta < 0) {
            delta = -delta;
        }

        const uint16_t bit = 1u << address;
        if (pressed_mask & bit) {
            if (delta < RELEASE_DELTA) {
                pressed_mask &= ~bit;
            }
        } else if (delta > PRESS_DELTA) {
            pressed_mask |= bit;
        }
    }

    if (!calibrated && ++baseline_frames >= HE_BASELINE_FRAMES) {
        finish_baseline();
    }

    return pressed_mask;
}

bool he_sensor_is_calibrated(void) {
    return calibrated;
}

uint32_t he_sensor_sample_errors(void) {
    return sample_errors;
}
