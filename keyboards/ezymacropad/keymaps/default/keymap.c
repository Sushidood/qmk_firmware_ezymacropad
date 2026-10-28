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

    [2] = LAYOUT(
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        KC_NO, KC_NO, KC_NO, KC_NO,
        TO(0), KC_NO, KC_NO, KC_NO
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
