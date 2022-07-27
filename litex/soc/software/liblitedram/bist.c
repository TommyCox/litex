// This file is Copyright (c) 2018-2020 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#include <generated/csr.h>
#include <generated/mem.h>
#if defined(CSR_SDRAM_GENERATOR_BASE) && defined(CSR_SDRAM_CHECKER_BASE)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <uart.h>
#include <time.h>
#include <console.h>

#include <liblitedram/bist.h>

#if !defined(MAIN_RAM_SIZE)
#define MAIN_RAM_SIZE 0x08000000
#endif

#define SDRAM_TEST_BASE 0x00000000
#define SDRAM_TEST_DATA_BYTES (CSR_SDRAM_DFII_PI0_RDDATA_SIZE*4)

#if defined(CSR_SDRAM_ECCR_BASE)
#define SDRAM_TEST_SIZE MAIN_RAM_SIZE/2
#else
#define SDRAM_TEST_SIZE MAIN_RAM_SIZE
#endif

/* If 0, generate sequential data; if 1, generate random data */
uint32_t rand_data = 1;

uint32_t wr_ticks;
uint32_t wr_length;
uint32_t rd_ticks;
uint32_t rd_length;
uint32_t rd_errors;

__attribute__((unused)) static void cdelay(int i)
{
#ifndef CONFIG_BIOS_NO_DELAYS
	while(i > 0) {
		__asm__ volatile(CONFIG_CPU_NOP);
		i--;
	}
#endif
}

#define PRB_SIZE (sizeof(pseudo_random_bases)/sizeof(pseudo_random_bases[0]))
static uint32_t pseudo_random_bases[] = {
	0x000e4018,0x0003338d,0x00233429,0x001f589d,
	0x001c922b,0x0011dc60,0x000d1e8f,0x000b20cf,
	0x00360188,0x00041174,0x0003d065,0x000bfe34,
	0x001bfc54,0x001dc7d5,0x00036587,0x00197383,
	0x0035b2d3,0x001c3765,0x00397fae,0x00239bc0,
	0x0000d4f3,0x00146fb7,0x0036183a,0x002b8d54,
	0x00239149,0x0013e6c0,0x001b8f66,0x002b1587,
	0x000d1539,0x000bdf18,0x0030a175,0x000c6133,
	0x002df309,0x002c06bd,0x0021dbd1,0x00058fc8,
	0x003ace6f,0x000ffa4d,0x003073d0,0x000a161f,
	0x002586dd,0x002e4a0e,0x00189ce9,0x0008e72e,
	0x0005dd92,0x001d2bc5,0x00250aaa,0x000a369f,
	0x001dcc17,0x000ced9d,0x0030a7f9,0x002394a3,
	0x003a0959,0x002eb2d2,0x0014d1d9,0x002f6217,
	0x002d7982,0x001ad120,0x00222c54,0x000923b7,
	0x0015e7df,0x001f55f6,0x0014ea5f,0x003b2b57,
	0x003091fe,0x00228da6,0x001c1c59,0x00298218,
	0x000728f9,0x001d5172,0x00041bdc,0x002860c3,
	0x0033595e,0x00224555,0x000878de,0x001b017c,
	0x0028475d,0x001b3758,0x003fe6cf,0x0032a410,
	0x003abba8,0x0012499d,0x0021e797,0x0011df68,
	0x001f917d,0x0021a184,0x0036d6eb,0x00331f8e,
	0x002e55e6,0x001c12b3,0x0011b4da,0x003f2b86,
	0x000ba2eb,0x000607e8,0x000e08fb,0x0013904d,
	0x00147a4a,0x00360956,0x000821ad,0x0031400e,
	0x0030d8e6,0x003be90f,0x00202e56,0x00017835,
	0x000ea9a1,0x00222753,0x002b8ade,0x000e4757,
	0x00259169,0x0037a663,0x00143e83,0x003a139e,
	0x00006a57,0x0021b6bb,0x0016de10,0x000d9ede,
	0x00263370,0x001975eb,0x0013903c,0x002fdc68,
	0x0014ada3,0x000012bd,0x00297df2,0x003e8aa1,
	0x00027e36,0x000e51ae,0x002e7627,0x00275c9f,
};

static inline void sdram_generator_init(uint32_t base, uint32_t length) {
	sdram_generator_reset_write(1);
	sdram_generator_reset_write(0);
	sdram_generator_random_write(rand_data);
	sdram_generator_base_write(base);
	sdram_generator_end_write(base + length);
	sdram_generator_length_write(length);
	cdelay(100);
}

static inline void sdram_checker_init(uint32_t base, uint32_t length) {
	sdram_checker_reset_write(1);
	sdram_checker_reset_write(0);
	sdram_checker_random_write(rand_data);
	sdram_checker_base_write(base);
	sdram_checker_end_write(base + length);
	sdram_checker_length_write(length);
	cdelay(100);
}

static void sdram_clear_counts(void) {
	wr_length = 0;
	wr_ticks  = 0;
	rd_length = 0;
	rd_ticks  = 0;
	rd_errors = 0;
}

#define TARGET_BYTES 0x20000
/* burst_length of 1, DMA transaction is about 468 cycles */
/* burst_length > 0x400, DMA transaction is about 1 cycle per word */

void sdram_bist_loop(uint32_t burst_length, uint32_t amode) {
	int i;
	uint32_t base = SDRAM_TEST_BASE;
	uint32_t length = burst_length*SDRAM_TEST_DATA_BYTES;
	uint32_t loop_cnt = (TARGET_BYTES > length) ? TARGET_BYTES / length : 1;

	rd_errors = 0;
	/* Prepare first write */
	sdram_generator_init(base, length);
	for (i = 0; i < loop_cnt; i++) {
		/* Start write */
		sdram_generator_start_write(1);
		/* Prepare next read */
		sdram_checker_init(base, length);
		/* Wait write */
		while (sdram_generator_done_read() == 0);
		/* Get write results */
		wr_ticks += sdram_generator_ticks_read();
		wr_length += length;
		/* Start read */
		sdram_checker_start_write(1);
		if (i != loop_cnt-1) {
			switch (amode) {
			case 1:
				base = base + length;
				if (base >= (SDRAM_TEST_BASE+SDRAM_TEST_SIZE)) base = SDRAM_TEST_BASE;
				break;
			case 2:
				base = SDRAM_TEST_BASE + pseudo_random_bases[i%PRB_SIZE]*SDRAM_TEST_DATA_BYTES;
				break;
			}
			/* Prepare next write */
			sdram_generator_init(base, length);
		}
		/* Wait read */
		while (sdram_checker_done_read() == 0);
		/* Get read results */
		rd_ticks  += sdram_checker_ticks_read();
		rd_errors += sdram_checker_errors_read();
		rd_length += length;
	}
}

static uint32_t compute_speed_mibs(uint32_t length, uint32_t ticks) {
	uint32_t speed;
	//printf("(%u, %u)", length, ticks);
	speed = (uint64_t)length*(CONFIG_CLOCK_FREQUENCY/(1024*1024))/ticks;
	return speed;
}

void sdram_bist(uint32_t burst_length, uint32_t amode)
{
	uint32_t i;
	uint32_t uc; /* update count */
	uint32_t uf; /* update frequency */
	uint32_t total_length;
	uint32_t total_errors;

	printf("Starting SDRAM BIST with burst_length=%lu and addr_mode=%lu\n", burst_length, amode);
#if defined(CSR_SDRAM_ECCR_BASE)
	sdram_eccr_clear_write(1);
	sdram_eccr_clear_write(0);
#endif
	sdram_clear_counts();
	i = 0;
	uc = 0;
	uf = burst_length * 2 + 2;
	total_length = 0;
	total_errors = 0;
	for (;;) {
		/* Exit on key pressed */
		if (readchar_nonblock())
			break;

		/* Header */
		if (uc%8 == 0 && wr_length == 0) {
#if defined(CSR_SDRAM_ECCR_BASE)
			printf("WR-BW(MiB/s) RD-BW(MiB/s)  TESTED(MiB)     ERRORS        SEC        DED\n");
#else
			printf("WR-BW(MiB/s) RD-BW(MiB/s)  TESTED(MiB)     ERRORS\n");
#endif
		}

		/* Bist loop */
		sdram_bist_loop(burst_length, amode);

		i++;
		/* Results, update about once per second */
		if ((burst_length < 0x100 && i%uf == 0) ||
			(wr_ticks >= CONFIG_CLOCK_FREQUENCY/2)) {
			uc++;
			total_length += wr_length;
			total_errors += rd_errors;

#if defined(CSR_SDRAM_ECCR_BASE)
			printf("%12lu %12lu %12lu %10lu %10lu %10lu\n",
				compute_speed_mibs(wr_length, wr_ticks),
				compute_speed_mibs(rd_length, rd_ticks),
				total_length/(1024*1024),
				total_errors,
				sdram_eccr_sec_errors_read(),
				sdram_eccr_ded_errors_read());
#else
			printf("%12lu %12lu %12lu %10lu\n",
				compute_speed_mibs(wr_length, wr_ticks),
				compute_speed_mibs(rd_length, rd_ticks),
				total_length/(1024*1024),
				total_errors);
#endif

			/* Clear length/ticks/errors */
			sdram_clear_counts();
		}
	}
}

#endif
