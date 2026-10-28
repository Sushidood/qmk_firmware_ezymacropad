#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Hall-effect acquisition engine: mux sequencing + ADC FIFO + DMA.
 *
 * Acquisition runs entirely from the DMA completion interrupt, concurrently
 * with the QMK main loop. For each mux channel the ISR selects the address,
 * arms DMA for one block of samples and starts the ADC free-running. When the
 * block lands, a second "stop" DMA channel (chained from the first) clears
 * ADC START_MANY in hardware, so at most one extra conversion can trail into
 * the FIFO regardless of ISR latency. Every block therefore begins with:
 *   [0] possibly the trailing conversion from the previous channel
 *   [1] possibly a conversion that started right as the mux switched
 * and both are discarded. Samples from HE_ADC_DISCARD_SAMPLES onward are
 * guaranteed to belong to this channel and to have had >= 1 full conversion
 * time (2 us) of mux settling.
 *
 * Completed frames are handed to the consumer through a triple buffer, so DMA
 * never waits for processing and the consumer only ever sees whole frames. */

#define HE_KEY_COUNT 16

#ifndef MUX_ADC_INPUT_PIN
#    define MUX_ADC_INPUT_PIN GP26
#endif

/* Leading samples dropped per channel. 2 is the minimum for correct
 * sample-to-channel mapping; raise it to add settling time (2 us each). */
#ifndef HE_ADC_DISCARD_SAMPLES
#    define HE_ADC_DISCARD_SAMPLES 2
#endif

/* Samples kept and summed per channel per frame. */
#ifndef HE_ADC_OVERSAMPLE
#    define HE_ADC_OVERSAMPLE 4
#endif

#define HE_ADC_BLOCK_SAMPLES (HE_ADC_DISCARD_SAMPLES + HE_ADC_OVERSAMPLE)

/* Set in a sample when the ADC flagged a conversion error. */
#define HE_ADC_SAMPLE_ERR 0x8000u

#ifndef HE_DMA_IRQ_PRIORITY
#    define HE_DMA_IRQ_PRIORITY 2
#endif

/* The engine is restarted if no frame completes for this long. */
#ifndef HE_ACQ_STALL_MS
#    define HE_ACQ_STALL_MS 10
#endif

/* Optional scope/logic-analyzer instrumentation, off unless defined.
 * HE_DEBUG_ISR_PIN is high while the acquisition ISR runs; HE_DEBUG_FRAME_PIN
 * toggles once per published frame, so its frequency is half the frame rate. */

/* samples[slot] holds the block for mux address he_scan_order[slot]. */
typedef struct {
    uint16_t samples[HE_KEY_COUNT][HE_ADC_BLOCK_SAMPLES];
} he_frame_t;

typedef struct {
    uint32_t frames;             /* frames published */
    uint32_t frames_overwritten; /* published frames replaced before the consumer took them */
    uint32_t dma_errors;
    uint32_t fifo_overflows;
    uint32_t restarts; /* watchdog restarts */
    uint32_t frame_us_last;
    uint32_t frame_us_min;
    uint32_t frame_us_max;
} he_acq_stats_t;

/* Mux addresses in scan order (Gray code, for low switching noise). */
extern const uint8_t he_scan_order[HE_KEY_COUNT];

/* Configures mux pins, ADC and DMA. Returns false if DMA channels are unavailable. */
bool he_acq_init(void);
void he_acq_start(void);

/* Returns the newest complete frame not yet taken, or NULL if none is ready.
 * The frame stays valid and untouched by DMA until the next call. */
const he_frame_t *he_acq_take_frame(void);

/* Call regularly from the main loop; restarts acquisition if it has stalled. */
void he_acq_poll_watchdog(void);

/* Copies the counters; optionally resets the min/max frame period. */
void he_acq_get_stats(he_acq_stats_t *out, bool reset_minmax);
