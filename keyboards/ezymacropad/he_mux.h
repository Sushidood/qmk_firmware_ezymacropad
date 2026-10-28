#pragma once

#include <stdint.h>
#include <hal.h>

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

#define HE_MUX_ADDRESS_COUNT 16

/* SIO set masks for every 4-bit mux address, precomputed by he_mux_init() so
 * that selecting a channel from the acquisition ISR is two register writes. */
extern uint32_t he_mux_set_masks[HE_MUX_ADDRESS_COUNT];
extern uint32_t he_mux_all_mask;

void he_mux_init(void);

/* Not atomic across the clear/set pair: only call from one context at a time
 * (the acquisition ISR, or with acquisition stopped). */
static inline void he_mux_select(uint8_t address) {
    SIO->GPIO_OUT_CLR = he_mux_all_mask & ~he_mux_set_masks[address];
    SIO->GPIO_OUT_SET = he_mux_set_masks[address];
}
