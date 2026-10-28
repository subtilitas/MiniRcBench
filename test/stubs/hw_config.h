// Host-test stub for the SD driver's board-configuration interface: the
// lookups by number. As in the pinned library, it includes sd_card.h.
#pragma once
#include "sd_card.h"

size_t      spi_get_num(void);
spi_t      *spi_get_by_num(size_t num);
size_t      sd_get_num(void);
sd_card_t  *sd_get_by_num(size_t num);
