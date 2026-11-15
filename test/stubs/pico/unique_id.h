// Host-test stub: the chip's unique ID, used for the USB serial number and
// the USB bridge's MSP_UID answer.
#pragma once
#include <stddef.h>
#include <stdint.h>
#define PICO_UNIQUE_BOARD_ID_SIZE_BYTES 8
typedef struct { uint8_t id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES]; } pico_unique_board_id_t;
void pico_get_unique_board_id(pico_unique_board_id_t *id_out);
void pico_get_unique_board_id_string(char *id_out, unsigned int len);
