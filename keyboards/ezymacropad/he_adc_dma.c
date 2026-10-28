#include "he_adc_dma.h"
#include "he_mux.h"
#include "gpio.h"
#include "timer.h"
#include <hal.h>

_Static_assert(HE_ADC_DISCARD_SAMPLES >= 2, "HE_ADC_DISCARD_SAMPLES < 2 breaks sample-to-channel mapping");
_Static_assert(HE_ADC_OVERSAMPLE >= 1, "HE_ADC_OVERSAMPLE must be at least 1");
_Static_assert(MUX_ADC_INPUT_PIN >= 26 && MUX_ADC_INPUT_PIN <= 29, "MUX_ADC_INPUT_PIN must be an ADC-capable pin (GP26-GP29)");

/* ADC register bits. Defined here because the ChibiOS ADC driver (which
 * normally provides them) is deliberately not built: this module owns the ADC. */
#define HE_ADC_CS_EN (1u << 0)
#define HE_ADC_CS_START_MANY (1u << 3)
#define HE_ADC_CS_READY (1u << 8)
#define HE_ADC_CS_AINSEL_POS 12
#define HE_ADC_FCS_EN (1u << 0)
#define HE_ADC_FCS_ERR (1u << 2)
#define HE_ADC_FCS_DREQ_EN (1u << 3)
#define HE_ADC_FCS_EMPTY (1u << 8)
#define HE_ADC_FCS_UNDER (1u << 10)
#define HE_ADC_FCS_OVER (1u << 11)
#define HE_ADC_FCS_THRESH_POS 24

/* FIFO on, DREQ on, per-sample error flag in bit 15, DREQ at 1 sample. */
#define HE_ADC_FCS_CONFIG (HE_ADC_FCS_EN | HE_ADC_FCS_DREQ_EN | HE_ADC_FCS_ERR | (1u << HE_ADC_FCS_THRESH_POS))

#define HE_BUFFER_COUNT 3
#define HE_BUFFER_NONE 0xFFu

const uint8_t he_scan_order[HE_KEY_COUNT] = {
    0, 1, 3, 2,
    6, 7, 5, 4,
    12, 13, 15, 14,
    10, 11, 9, 8,
};

static he_frame_t acq_frames[HE_BUFFER_COUNT];

/* Triple-buffer ownership. The ISR owns acq_filling, the consumer owns
 * acq_reading, and acq_ready is the hand-off slot. The three are always
 * distinct (or ready is NONE). The consumer only touches acq_ready with
 * interrupts masked; the ISR cannot be preempted by the consumer. */
static volatile uint8_t acq_filling = 0;
static volatile uint8_t acq_ready   = HE_BUFFER_NONE;
static volatile uint8_t acq_reading = 1;

static uint8_t acq_slot;

static const rp_dma_channel_t *acq_data_dma;
static const rp_dma_channel_t *acq_stop_dma;
static uint32_t                acq_data_mode;

/* Source word for the stop channel, written to the ADC CS clear alias. Kept
 * in RAM (not const/flash) so DMA can still read it while flash is being
 * written for EEPROM emulation. */
static uint32_t acq_stop_word = HE_ADC_CS_START_MANY;

static volatile he_acq_stats_t acq_stats;
static uint32_t                acq_last_frame_us;

static uint32_t acq_watchdog_frames;
static uint32_t acq_watchdog_time;

#ifdef HE_DEBUG_ISR_PIN
#    define DEBUG_ISR_HIGH() (SIO->GPIO_OUT_SET = 1u << HE_DEBUG_ISR_PIN)
#    define DEBUG_ISR_LOW() (SIO->GPIO_OUT_CLR = 1u << HE_DEBUG_ISR_PIN)
#else
#    define DEBUG_ISR_HIGH()
#    define DEBUG_ISR_LOW()
#endif

static inline void acq_start_slot(uint8_t slot) {
    he_mux_select(he_scan_order[slot]);
    acq_data_dma->channel->WRITE_ADDR  = (uint32_t)acq_frames[acq_filling].samples[slot];
    acq_data_dma->channel->TRANS_COUNT = HE_ADC_BLOCK_SAMPLES;
    acq_data_dma->channel->CTRL_TRIG   = acq_data_mode;
    ADC->SET.CS                        = HE_ADC_CS_START_MANY;
}

/* Stops the ADC and DMA, empties the FIFO and starts a fresh frame at slot 0.
 * Used at start-up, on errors and by the watchdog. Must run with interrupts
 * masked or from the acquisition ISR. Busy-waits for at most one conversion. */
static void acq_resync_and_start(void) {
    ADC->CLR.CS = HE_ADC_CS_START_MANY;

    /* Abort with the channel's interrupt masked so a spurious completion
     * (RP2040-E13) cannot re-enter the ISR. */
    dmaChannelDisableInterruptX(acq_data_dma);
    acq_data_dma->channel->CTRL_TRIG &= ~DMA_CTRL_TRIG_EN;
    dmaChannelAbortX(acq_data_dma);
    DMA->INTS0 = acq_data_dma->chnmask;
    dmaChannelEnableInterruptX(acq_data_dma);

    while (!(ADC->CS & HE_ADC_CS_READY)) {
    }
    while (!(ADC->FCS & HE_ADC_FCS_EMPTY)) {
        (void)ADC->FIFO;
    }
    ADC->FCS = HE_ADC_FCS_CONFIG | HE_ADC_FCS_OVER | HE_ADC_FCS_UNDER;

    acq_slot = 0;
    acq_start_slot(0);
}

static void acq_publish_frame(void) {
    if (acq_ready == HE_BUFFER_NONE) {
        acq_ready   = acq_filling;
        acq_filling = (uint8_t)(HE_BUFFER_COUNT * (HE_BUFFER_COUNT - 1) / 2 - acq_ready - acq_reading);
    } else {
        /* Consumer has not taken the last frame: replace it with this newer one. */
        const uint8_t stale = acq_ready;
        acq_ready           = acq_filling;
        acq_filling         = stale;
        acq_stats.frames_overwritten++;
    }
    acq_stats.frames++;

    const uint32_t now    = TIMER->TIMERAWL;
    const uint32_t period = now - acq_last_frame_us;
    acq_last_frame_us     = now;
    acq_stats.frame_us_last = period;
    if (period < acq_stats.frame_us_min) {
        acq_stats.frame_us_min = period;
    }
    if (period > acq_stats.frame_us_max) {
        acq_stats.frame_us_max = period;
    }

#ifdef HE_DEBUG_FRAME_PIN
    SIO->GPIO_OUT_XOR = 1u << HE_DEBUG_FRAME_PIN;
#endif
}

static void acq_dma_complete(void *param, uint32_t ctrl) {
    (void)param;
    DEBUG_ISR_HIGH();

    if (ctrl & DMA_CTRL_TRIG_AHB_ERROR) {
        acq_stats.dma_errors++;
        acq_resync_and_start();
    } else if (ADC->FCS & HE_ADC_FCS_OVER) {
        /* Samples were lost, so this block's alignment can't be trusted. */
        acq_stats.fifo_overflows++;
        acq_resync_and_start();
    } else {
        if (++acq_slot == HE_KEY_COUNT) {
            acq_publish_frame();
            acq_slot = 0;
        }
        acq_start_slot(acq_slot);
    }

    DEBUG_ISR_LOW();
}

bool he_acq_init(void) {
    he_mux_init();

#ifdef HE_DEBUG_ISR_PIN
    gpio_set_pin_output_push_pull(HE_DEBUG_ISR_PIN);
    gpio_write_pin_low(HE_DEBUG_ISR_PIN);
#endif
#ifdef HE_DEBUG_FRAME_PIN
    gpio_set_pin_output_push_pull(HE_DEBUG_FRAME_PIN);
    gpio_write_pin_low(HE_DEBUG_FRAME_PIN);
#endif

    palSetLineMode(MUX_ADC_INPUT_PIN, PAL_MODE_INPUT_ANALOG);

    hal_lld_peripheral_reset(RESETS_ALLREG_ADC);
    hal_lld_peripheral_unreset(RESETS_ALLREG_ADC);
    ADC->CS = HE_ADC_CS_EN | ((uint32_t)(MUX_ADC_INPUT_PIN - 26) << HE_ADC_CS_AINSEL_POS);
    while (!(ADC->CS & HE_ADC_CS_READY)) {
    }
    ADC->DIV = 0; /* back-to-back conversions: 96 ADC clocks = 2 us at 48 MHz */
    ADC->FCS = HE_ADC_FCS_CONFIG;

    acq_data_dma = dmaChannelAlloc(RP_DMA_CHANNEL_ID_ANY, HE_DMA_IRQ_PRIORITY, acq_dma_complete, NULL);
    acq_stop_dma = dmaChannelAlloc(RP_DMA_CHANNEL_ID_ANY, HE_DMA_IRQ_PRIORITY, NULL, NULL);
    if (acq_data_dma == NULL || acq_stop_dma == NULL) {
        return false;
    }

    /* Stop channel: one word into the ADC CS atomic-clear alias, triggered
     * only by the data channel's chain. Written via AL1_CTRL so it doesn't start now. */
    acq_stop_dma->channel->READ_ADDR   = (uint32_t)&acq_stop_word;
    acq_stop_dma->channel->WRITE_ADDR  = (uint32_t)&ADC->CLR.CS;
    acq_stop_dma->channel->TRANS_COUNT = 1;
    acq_stop_dma->channel->AL1_CTRL    = DMA_CTRL_TRIG_EN | DMA_CTRL_TRIG_HIGH_PRIORITY | DMA_CTRL_TRIG_DATA_SIZE_WORD | DMA_CTRL_TRIG_TREQ_PERMANENT | DMA_CTRL_TRIG_IRQ_QUIET | DMA_CTRL_TRIG_CHAIN_TO(acq_stop_dma->chnidx);

    /* Data channel: ADC FIFO -> frame buffer, paced by the ADC DREQ. */
    acq_data_dma->channel->READ_ADDR = (uint32_t)&ADC->FIFO;
    acq_data_mode                    = DMA_CTRL_TRIG_EN | DMA_CTRL_TRIG_HIGH_PRIORITY | DMA_CTRL_TRIG_DATA_SIZE_HWORD | DMA_CTRL_TRIG_INCR_WRITE | DMA_CTRL_TRIG_TREQ_ADC | DMA_CTRL_TRIG_CHAIN_TO(acq_stop_dma->chnidx);
    dmaChannelEnableInterruptX(acq_data_dma);

    acq_stats.frame_us_min = UINT32_MAX;
    return true;
}

void he_acq_start(void) {
    chSysLock();
    acq_last_frame_us = TIMER->TIMERAWL;
    acq_resync_and_start();
    chSysUnlock();

    acq_watchdog_frames = 0;
    acq_watchdog_time   = timer_read32();
}

const he_frame_t *he_acq_take_frame(void) {
    uint8_t taken;

    chSysLock();
    taken = acq_ready;
    if (taken != HE_BUFFER_NONE) {
        acq_reading = taken;
        acq_ready   = HE_BUFFER_NONE;
    }
    chSysUnlock();

    return (taken == HE_BUFFER_NONE) ? NULL : &acq_frames[taken];
}

void he_acq_poll_watchdog(void) {
    const uint32_t frames = acq_stats.frames;
    if (frames != acq_watchdog_frames) {
        acq_watchdog_frames = frames;
        acq_watchdog_time   = timer_read32();
        return;
    }

    if (timer_elapsed32(acq_watchdog_time) >= HE_ACQ_STALL_MS) {
        chSysLock();
        acq_stats.restarts++;
        acq_resync_and_start();
        chSysUnlock();
        acq_watchdog_time = timer_read32();
    }
}

void he_acq_get_stats(he_acq_stats_t *out, bool reset_minmax) {
    chSysLock();
    out->frames             = acq_stats.frames;
    out->frames_overwritten = acq_stats.frames_overwritten;
    out->dma_errors         = acq_stats.dma_errors;
    out->fifo_overflows     = acq_stats.fifo_overflows;
    out->restarts           = acq_stats.restarts;
    out->frame_us_last      = acq_stats.frame_us_last;
    out->frame_us_min       = acq_stats.frame_us_min;
    out->frame_us_max       = acq_stats.frame_us_max;
    if (reset_minmax) {
        acq_stats.frame_us_min = UINT32_MAX;
        acq_stats.frame_us_max = 0;
    }
    chSysUnlock();
}
