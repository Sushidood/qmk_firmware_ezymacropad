#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [0] = LAYOUT(
        KC_VOLD, KC_VOLU, KC_BRIU, KC_P9,
        KC_MPRV, KC_MNXT, KC_BRID, KC_P3,
        KC_MUTE, LCTL(KC_C), KC_UP, LCTL(KC_V),
        MO(1), KC_LEFT, KC_DOWN, KC_RGHT
    ),

    [1] = LAYOUT(
        TO(1), TO(2), TO(3), TO(4),
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_TRNS, KC_NO, KC_NO, KC_SLEP
    ),

    /* MIDI mode (hold MO(1), tap the second key). One chromatic octave laid
     * out bottom-up from C, octave down/up, and EZ_VTOG to switch between
     * velocity from key speed and fixed velocity. TO(0) goes back. */
    [2] = LAYOUT(
        MI_Gs, MI_A,    MI_As,   MI_B,
        MI_E,  MI_F,    MI_Fs,   MI_G,
        MI_C,  MI_Cs,   MI_D,    MI_Ds,
        TO(0), MI_OCTD, MI_OCTU, EZ_VTOG
    ),

    [3] = LAYOUT(
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        TO(0), KC_NO, KC_NO, KC_NO
    ),

    [4] = LAYOUT(
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        TO(0), KC_NO, KC_NO, KC_NO
    )
};
