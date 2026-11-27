#include "he_sensor.h"
#ifdef HE_TRAVEL_TIMING
#    include <hal.h>
#endif

_Static_assert(MUX_ANALOG_THRESHOLD > MUX_ANALOG_HYSTERESIS, "MUX_ANALOG_HYSTERESIS must be smaller than MUX_ANALOG_THRESHOLD");

/* The thresholds are in 10-bit counts (what analogReadPin returned when they
 * were tuned), but the DMA engine stores raw 12-bit samples, so scale by 4.
 * Everything below works on sums of HE_ADC_OVERSAMPLE samples, so the
 * thresholds are scaled once here instead of dividing every reading. */
#define HE_THRESHOLD_SCALE 4
#define PRESS_DELTA ((int32_t)MUX_ANALOG_THRESHOLD * HE_THRESHOLD_SCALE * HE_ADC_OVERSAMPLE)
#define RELEASE_DELTA ((int32_t)(MUX_ANALOG_THRESHOLD - MUX_ANALOG_HYSTERESIS) * HE_THRESHOLD_SCALE * HE_ADC_OVERSAMPLE)

#ifdef HE_TRAVEL_TIMING
_Static_assert(HE_TRAVEL_START_THRESHOLD < MUX_ANALOG_THRESHOLD - MUX_ANALOG_HYSTERESIS, "HE_TRAVEL_START_THRESHOLD must be below the release point");
#    define TRAVEL_START_DELTA ((int32_t)HE_TRAVEL_START_THRESHOLD * HE_THRESHOLD_SCALE * HE_ADC_OVERSAMPLE)
#endif

/* All per-key arrays are indexed by mux address. */
static uint32_t baseline_acc[HE_KEY_COUNT];
static uint16_t baseline_count[HE_KEY_COUNT];
static int32_t  baseline[HE_KEY_COUNT];
static uint16_t baseline_frames;
static bool     calibrated;
static uint16_t pressed_mask;
static uint32_t sample_errors;
#ifdef HE_TRAVEL_TIMING
static uint16_t travel_armed_mask;           /* key has left rest and the clock is running */
static uint32_t travel_start[HE_KEY_COUNT];  /* TIMERAWL when it left rest */
static uint32_t travel_us[HE_KEY_COUNT];     /* rest -> actuation time of the last press */
#endif

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
#ifdef HE_TRAVEL_TIMING
    travel_armed_mask = 0;
    for (uint8_t i = 0; i < HE_KEY_COUNT; i++) {
        travel_start[i] = 0;
        travel_us[i]    = 0;
    }
#endif
}

static void finish_baseline(void) {
    for (uint8_t i = 0; i < HE_KEY_COUNT; i++) {
        baseline[i] = baseline_count[i] ? (int32_t)(baseline_acc[i] / baseline_count[i]) : 0;
    }
    calibrated = true;
}

uint16_t he_sensor_process(const he_frame_t *frame) {
#ifdef HE_TRAVEL_TIMING
    /* One timestamp per frame: the frame is at most one frame period (~200 us)
     * old, which is well below the stroke times being measured. */
    const uint32_t now = TIMER->TIMERAWL;
#endif
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
#ifdef HE_TRAVEL_TIMING
                /* Restart the clock from the release point so a quick
                 * re-press (trill) without returning to rest is timed too. */
                if (delta > TRAVEL_START_DELTA) {
                    travel_armed_mask |= bit;
                    travel_start[address] = now;
                } else {
                    travel_armed_mask &= ~bit;
                }
#endif
            }
        } else if (delta > PRESS_DELTA) {
            pressed_mask |= bit;
#ifdef HE_TRAVEL_TIMING
            travel_us[address] = (travel_armed_mask & bit) ? now - travel_start[address] : 0;
            travel_armed_mask &= ~bit;
#endif
        }
#ifdef HE_TRAVEL_TIMING
        else if (delta > TRAVEL_START_DELTA) {
            if (!(travel_armed_mask & bit)) {
                travel_armed_mask |= bit;
                travel_start[address] = now;
            }
        } else {
            travel_armed_mask &= ~bit;
        }
#endif
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

#ifdef HE_TRAVEL_TIMING
uint32_t he_sensor_travel_us(uint8_t address) {
    return address < HE_KEY_COUNT ? travel_us[address] : 0;
}
#endif
