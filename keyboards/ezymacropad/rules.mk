# This board uses a custom matrix scan for the Hall-effect multiplexer.
CUSTOM_MATRIX = lite
SRC += matrix.c

# Enable QMK ADC driver for analogReadPin() yaaay
ANALOG_DRIVER_REQUIRED = yes
RAW_ENABLE = yes
