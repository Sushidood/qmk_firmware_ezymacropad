# This board uses a custom matrix scan for the Hall-effect multiplexer.
CUSTOM_MATRIX = lite
SRC += matrix.c he_mux.c he_adc_dma.c he_sensor.c

# The acquisition engine drives the ADC directly with DMA, so QMK's analog
# driver (analogReadPin) is not used. Enable the ChibiOS RP2040 DMA allocator.
OPT_DEFS += -DRP_DMA_REQUIRED=TRUE
RAW_ENABLE = yes
