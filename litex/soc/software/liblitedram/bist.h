// This file is Copyright (c) 2018-2020 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#ifndef __SDRAM_BIST_H
#define __SDRAM_BIST_H

#include <stdint.h>

typedef enum {MD_FIX, MD_INC, MD_RAN} sequence_t;

void sdram_bist_pattern(uint32_t value);
void sdram_bist_gen(uint32_t base, uint32_t length, int dmode);
void sdram_bist_chk(uint32_t base, uint32_t length, int dmode);
void sdram_bist(uint32_t length, int amode, int dmode, int wmode);

#endif /* __SDRAM_BIST_H */
