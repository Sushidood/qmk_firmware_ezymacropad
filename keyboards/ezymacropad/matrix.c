#include "quantum.h"
#include "matrix.h"
#include "gpio.h"
#include "analog.h"
#include "wait.h"
#include <string.h>

#ifndef MUX_SELECT_0_PIN
#    define MUX_SELECT_0_PIN GP5
#endif
#ifndef MUX_SELECT_1_PIN
#    define MUX_SELECT_1_PIN GP4
#endif
#ifndef MUX_SELECT_2_PIN
#    define MUX_SELECT_2_PIN GP2
#endif
#ifndef MUX_SELECT_3_PIN
#    define MUX_SELECT_3_PIN GP3
#endif

#ifndef MUX_SETTLE_US
#    define MUX_SETTLE_US 1
#endif

#ifndef MUX_KEY_COUNT
#    define MUX_KEY_COUNT 16
#endif

#ifndef MUX_ADC_INPUT_PIN
#    define MUX_ADC_INPUT_PIN GP26
#endif

#ifndef MUX_ANALOG_THRESHOLD
#    define MUX_ANALOG_THRESHOLD 35
#endif

#ifndef MUX_ANALOG_HYSTERESIS
#    define MUX_ANALOG_HYSTERESIS 12
#endif

static const pin_t mux_select_pins[4] = {
    MUX_SELECT_0_PIN,
    MUX_SELECT_1_PIN,
    MUX_SELECT_2_PIN,
    MUX_SELECT_3_PIN,
};

/* Physical hardware layout of the muxed channels. This is not the scan order.
 * The rows are laid out as given by the board, and the MUX itself is selected
 * by 4-bit binary addresses. We scan in Gray-code order so the address pins do
 * not toggle as aggressively, which reduces switching noise. */
static const uint8_t mux_physical_layout[MATRIX_ROWS][MATRIX_COLS] = {
    {13, 14, 15, 16},
    {6, 8, 11, 12},
    {4, 5, 7, 9},
    {1, 2, 3, 10},
};

/* keymap_remap[row][col] defines the logical matrix position that a physical
 * key should report as. This table matches the board's physical mux layout,
 * while the scan order itself remains Gray-code for low switching noise.
 *
 * Physical rows are:
 *   [13, 14, 15, 16]
 *   [6, 8, 11, 12]
 *   [4, 5, 7, 9]
 *   [1, 2, 3, 10]
 */
static const uint8_t keymap_remap[MATRIX_ROWS][MATRIX_COLS] = {
    {0, 1, 2, 3},   /* 13, 14, 15, 16 */
    {4, 5, 6, 7},   /* 6, 8, 11, 12 */
    {8, 9, 10, 11}, /* 4, 5, 7, 9 */
    {12, 13, 14, 15} /* 1, 2, 3, 10 */
};

/* Gray-code order is the electrically quietest sequence for a 4-bit mux. */
static const uint8_t mux_gray_scan_order[MUX_KEY_COUNT] = {
    0, 1, 3, 2,
    6, 7, 5, 4,
    12, 13, 15, 14,
    10, 11, 9, 8,
};

static uint8_t mux_address_to_matrix_index[16];
static uint16_t mux_baseline[MUX_KEY_COUNT] = {0};
static uint16_t mux_threshold[MUX_KEY_COUNT] = {0};
static bool     mux_pressed_state[MUX_KEY_COUNT] = {false};
static bool     mux_analog_mode = false;

static uint8_t mux_channel_to_address(uint8_t channel) {
    /* The hardware is 4-bit addressed; a physical channel value of 16 maps to
     * address 0 in the 4-bit decode domain. */
    return (channel == 16u) ? 0u : (channel & 0x0Fu);
}

static void remap_physical_key(uint8_t physical_row, uint8_t physical_col, uint8_t *logical_row, uint8_t *logical_col) {
    const uint8_t logical_index = keymap_remap[physical_row][physical_col];
    if (logical_row != NULL) {
        *logical_row = logical_index / MATRIX_COLS;
    }
    if (logical_col != NULL) {
        *logical_col = logical_index % MATRIX_COLS;
    }
}

static void mux_build_address_map(void) {
    for (uint8_t i = 0; i < 16; ++i) {
        mux_address_to_matrix_index[i] = 0xFF;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            const uint8_t channel = mux_physical_layout[row][col];
            const uint8_t address = mux_channel_to_address(channel);
            const uint8_t matrix_index = (row * MATRIX_COLS) + col;
            mux_address_to_matrix_index[address] = matrix_index;
        }
    }
}

static void mux_set_address(uint8_t address) {
    for (uint8_t i = 0; i < 4; i++) {
        gpio_write_pin(mux_select_pins[i], (address >> i) & 0x1u);
    }
    wait_us(MUX_SETTLE_US);
}

static void mux_calibrate_one(uint8_t key_index) {
    uint32_t sum = 0;
    uint8_t  samples = 4;

    for (uint8_t i = 0; i < samples; i++) {
        mux_set_address(key_index);
        sum += analogReadPin(MUX_ADC_INPUT_PIN);
        wait_us(5);
    }

    mux_baseline[key_index] = (uint16_t)(sum / samples);
    mux_threshold[key_index] = MUX_ANALOG_THRESHOLD;
}

static void mux_calibrate_all(void) {
    for (uint8_t key_index = 0; key_index < MUX_KEY_COUNT; key_index++) {
        mux_calibrate_one(key_index);
    }
}

static bool mux_read_analog_key(uint8_t key_index, uint16_t *value) {
    mux_set_address(key_index);
    wait_us(MUX_SETTLE_US);

    if (value == NULL) {
        return false;
    }

    *value = analogReadPin(MUX_ADC_INPUT_PIN);
    return true;
}

void matrix_init_custom(void) {
    for (uint8_t i = 0; i < 4; i++) {
        gpio_set_pin_output_push_pull(mux_select_pins[i]);
        gpio_write_pin_low(mux_select_pins[i]);
    }

    mux_build_address_map();
    mux_set_address(0);

    mux_analog_mode = (MUX_ADC_INPUT_PIN != NO_PIN);
    if (mux_analog_mode) {
        mux_calibrate_all();
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    matrix_row_t curr_matrix[MATRIX_ROWS] = {0};

    for (uint8_t scan_index = 0; scan_index < MUX_KEY_COUNT; ++scan_index) {
        const uint8_t address = mux_gray_scan_order[scan_index];
        const uint8_t matrix_index = mux_address_to_matrix_index[address];
        if (matrix_index == 0xFF) {
            continue;
        }

        const uint8_t physical_row = matrix_index / MATRIX_COLS;
        const uint8_t physical_col = matrix_index % MATRIX_COLS;
        uint8_t logical_row = 0;
        uint8_t logical_col = 0;
        remap_physical_key(physical_row, physical_col, &logical_row, &logical_col);
        const matrix_row_t bit = (matrix_row_t)1 << logical_col;

        bool pressed = false;
        if (mux_analog_mode) {
            uint16_t value = 0;
            if (mux_read_analog_key(address, &value)) {
                const uint16_t baseline = mux_baseline[address];
                const uint16_t threshold = mux_threshold[address];
                const int delta = (int)value - (int)baseline;
                const int abs_delta = delta < 0 ? -delta : delta;

                if (abs_delta > threshold) {
                    pressed = true;
                } else if (mux_pressed_state[matrix_index]) {
                    if (abs_delta < (int)(threshold - MUX_ANALOG_HYSTERESIS)) {
                        pressed = false;
                    }
                }
            }
        }

        if (pressed) {
            curr_matrix[logical_row] |= bit;
        }
        mux_pressed_state[matrix_index] = pressed;
    }

    bool changed = memcmp(current_matrix, curr_matrix, sizeof(curr_matrix)) != 0;
    if (changed) {
        memcpy(current_matrix, curr_matrix, sizeof(curr_matrix));
    }

    return changed;
}
