#include "he_mux.h"
#include "gpio.h"

static const pin_t mux_select_pins[4] = {
    MUX_SELECT_0_PIN,
    MUX_SELECT_1_PIN,
    MUX_SELECT_2_PIN,
    MUX_SELECT_3_PIN,
};

uint32_t he_mux_set_masks[HE_MUX_ADDRESS_COUNT];
uint32_t he_mux_all_mask;

void he_mux_init(void) {
    he_mux_all_mask = 0;
    for (uint8_t i = 0; i < 4; i++) {
        gpio_set_pin_output_push_pull(mux_select_pins[i]);
        gpio_write_pin_low(mux_select_pins[i]);
        he_mux_all_mask |= 1u << mux_select_pins[i];
    }

    for (uint8_t address = 0; address < HE_MUX_ADDRESS_COUNT; address++) {
        uint32_t mask = 0;
        for (uint8_t bit = 0; bit < 4; bit++) {
            if (address & (1u << bit)) {
                mask |= 1u << mux_select_pins[bit];
            }
        }
        he_mux_set_masks[address] = mask;
    }

    he_mux_select(0);
}
