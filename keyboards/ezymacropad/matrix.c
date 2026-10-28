#include "quantum.h"
#include "matrix.h"
#include "he_adc_dma.h"
#include "he_sensor.h"
#include <string.h>

#if defined(HE_STATS_ENABLE) && defined(CONSOLE_ENABLE)
#    include <hal.h>
#    include "print.h"
#    ifndef HE_STATS_INTERVAL_MS
#        define HE_STATS_INTERVAL_MS 1000
#    endif
#endif

/* Physical hardware layout of the muxed channels. This is not the scan order;
 * the acquisition engine scans in Gray-code order (he_scan_order). */
static const uint8_t mux_physical_layout[MATRIX_ROWS][MATRIX_COLS] = {
    {13, 14, 15, 16},
    {6, 8, 11, 12},
    {4, 5, 7, 9},
    {1, 2, 3, 10},
};

/* keymap_remap[row][col] defines the logical matrix position that a physical
 * key should report as.
 *
 * Physical rows are:
 *   [13, 14, 15, 16]
 *   [6, 8, 11, 12]
 *   [4, 5, 7, 9]
 *   [1, 2, 3, 10]
 */
static const uint8_t keymap_remap[MATRIX_ROWS][MATRIX_COLS] = {
    {1, 2, 3, 12},   /* 13, 14, 15, 16 */
    {10, 11, 7, 0},  /* 6, 8, 11, 12 */
    {9, 4, 5, 15},   /* 4, 5, 7, 9 */
    {13, 14, 8, 6}   /* 1, 2, 3, 10 */
};

/* Mux address -> logical matrix position, built once from the tables above. */
static uint8_t      address_row[HE_KEY_COUNT];
static matrix_row_t address_col_bit[HE_KEY_COUNT];
static uint16_t     address_used_mask;

static bool     acq_ok;
static uint16_t last_pressed;

static uint8_t mux_channel_to_address(uint8_t channel) {
    /* The hardware is 4-bit addressed; a physical channel value of 16 maps to
     * address 0 in the 4-bit decode domain. */
    return (channel == 16u) ? 0u : (channel & 0x0Fu);
}

static void build_address_map(void) {
    address_used_mask = 0;
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            const uint8_t address       = mux_channel_to_address(mux_physical_layout[row][col]);
            const uint8_t logical_index = keymap_remap[row][col];
            address_row[address]        = logical_index / MATRIX_COLS;
            address_col_bit[address]    = (matrix_row_t)1 << (logical_index % MATRIX_COLS);
            address_used_mask |= 1u << address;
        }
    }
}

#if defined(HE_STATS_ENABLE) && defined(CONSOLE_ENABLE)
static uint32_t stats_timer;
static uint32_t stats_last_frames;
static uint32_t stats_process_us_max;

static void print_stats(void) {
    if (timer_elapsed32(stats_timer) < HE_STATS_INTERVAL_MS) {
        return;
    }
    const uint32_t elapsed = timer_elapsed32(stats_timer);
    stats_timer            = timer_read32();

    he_acq_stats_t s;
    he_acq_get_stats(&s, true);
    const uint32_t fps = (s.frames - stats_last_frames) * 1000u / elapsed;
    stats_last_frames  = s.frames;

    uprintf("he: %lu fps, frame %lu/%lu/%lu us (last/min/max), proc max %lu us, overwritten %lu, dma err %lu, ovf %lu, restarts %lu, adc err %lu, cal %u\n",
            fps, s.frame_us_last, s.frame_us_min, s.frame_us_max, stats_process_us_max, s.frames_overwritten, s.dma_errors, s.fifo_overflows, s.restarts, he_sensor_sample_errors(), he_sensor_is_calibrated());
    stats_process_us_max = 0;
}
#endif

#if !defined(SERIAL_NUMBER) && defined(SERIAL_NUMBER_USE_HARDWARE_ID) && SERIAL_NUMBER_USE_HARDWARE_ID == TRUE
/* QMK builds the USB serial string from the flash unique ID the first time the
 * host asks for it, inside the USB ISR. That read switches flash execution (XIP)
 * off for ~100 us, and the acquisition IRQ (higher priority, every ~14 us, code
 * and vectors in flash) would run during it and crash the MCU mid-enumeration.
 * Build the string once here, with interrupts masked and before acquisition
 * starts, so the USB ISR never touches flash again. */
void set_serial_number_descriptor(void);
#    define HE_PRELOAD_SERIAL_NUMBER
#endif

void matrix_init_custom(void) {
    build_address_map();
    he_sensor_init();

#ifdef HE_PRELOAD_SERIAL_NUMBER
    chSysLock();
    set_serial_number_descriptor();
    chSysUnlock();
#endif

    acq_ok = he_acq_init();
    if (acq_ok) {
        he_acq_start();
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    if (!acq_ok) {
        return false;
    }

    he_acq_poll_watchdog();
#if defined(HE_STATS_ENABLE) && defined(CONSOLE_ENABLE)
    print_stats();
#endif

    const he_frame_t *frame = he_acq_take_frame();
    if (frame == NULL) {
        return false;
    }

#if defined(HE_STATS_ENABLE) && defined(CONSOLE_ENABLE)
    const uint32_t process_start = TIMER->TIMERAWL;
#endif
    const uint16_t pressed = he_sensor_process(frame) & address_used_mask;
#if defined(HE_STATS_ENABLE) && defined(CONSOLE_ENABLE)
    const uint32_t process_us = TIMER->TIMERAWL - process_start;
    if (process_us > stats_process_us_max) {
        stats_process_us_max = process_us;
    }
#endif

    if (pressed == last_pressed) {
        return false;
    }
    last_pressed = pressed;

    matrix_row_t curr_matrix[MATRIX_ROWS] = {0};
    for (uint8_t address = 0; address < HE_KEY_COUNT; ++address) {
        if (pressed & (1u << address)) {
            curr_matrix[address_row[address]] |= address_col_bit[address];
        }
    }

    bool changed = memcmp(current_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
    if (changed) {
        memcpy(current_matrix, curr_matrix, sizeof(curr_matrix));
    }

    return changed;
}
