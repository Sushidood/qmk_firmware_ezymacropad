// Copyright 2026 Sushidood
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#if defined(MIDI_ENABLE) && defined(MIDI_ADVANCED)
#    include "process_midi.h"
#    include "he_sensor.h"
#    include <math.h>

/* MIDI velocity from key travel speed.
 *
 * The Hall sensors report how far each key is down, so he_sensor.c times how
 * long a key takes from leaving rest to actuating. That time is turned into a
 * note-on velocity here: a fast strike plays loud, a slow press plays soft.
 * QMK's MIDI code (process_midi) sends the note using midi_config.velocity, so
 * it is swapped in just for that one key press and put back afterwards. The
 * fixed velocity (MI_VELU / MI_VELD / MI_VEL_x) is used for note-offs, and for
 * note-ons too while velocity sensing is switched off with EZ_VTOG. */

#    ifdef HE_TRAVEL_TIMING

uint32_t ezymacropad_key_travel_us(uint8_t row, uint8_t col);

static bool    dynamic_velocity = true;
static bool    velocity_swapped;
static uint8_t saved_velocity;

static uint8_t velocity_from_travel(uint32_t travel_us) {
    if (travel_us <= EZY_MIDI_VELOCITY_FAST_US) {
        return EZY_MIDI_VELOCITY_MAX;
    }
    if (travel_us >= EZY_MIDI_VELOCITY_SLOW_US) {
        return EZY_MIDI_VELOCITY_MIN;
    }
    /* Logarithmic in time, so halving the stroke time always adds the same
     * amount of velocity. This feels even across soft and hard playing. */
    const float position = logf((float)travel_us / EZY_MIDI_VELOCITY_FAST_US) / logf((float)EZY_MIDI_VELOCITY_SLOW_US / EZY_MIDI_VELOCITY_FAST_US);
    const float velocity = EZY_MIDI_VELOCITY_MAX - position * (EZY_MIDI_VELOCITY_MAX - EZY_MIDI_VELOCITY_MIN);
    return (uint8_t)(velocity + 0.5f);
}

#    endif // HE_TRAVEL_TIMING

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#    ifdef HE_TRAVEL_TIMING
    if (keycode == EZ_VTOG) {
        if (record->event.pressed) {
            dynamic_velocity = !dynamic_velocity;
        }
        return false;
    }
    if (dynamic_velocity && record->event.pressed && keycode >= MIDI_TONE_MIN && keycode <= MIDI_TONE_MAX) {
        const uint32_t travel_us = ezymacropad_key_travel_us(record->event.key.row, record->event.key.col);
        const uint8_t  velocity  = velocity_from_travel(travel_us);
#        if defined(EZY_MIDI_VELOCITY_DEBUG) && defined(CONSOLE_ENABLE)
        uprintf("midi: key %u,%u travel %lu us -> velocity %u\n", record->event.key.row, record->event.key.col, travel_us, velocity);
#        endif
        saved_velocity       = midi_config.velocity;
        midi_config.velocity = velocity;
        velocity_swapped     = true;
    }
#    else
    if (keycode == EZ_VTOG) {
        return false;
    }
#    endif
    return process_record_user(keycode, record);
}

void post_process_record_kb(uint16_t keycode, keyrecord_t *record) {
#    ifdef HE_TRAVEL_TIMING
    if (velocity_swapped) {
        midi_config.velocity = saved_velocity;
        velocity_swapped     = false;
    }
#    endif
    post_process_record_user(keycode, record);
}

#endif // MIDI_ENABLE && MIDI_ADVANCED
