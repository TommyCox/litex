// This file is Copyright (c) 2018-2020 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#include <generated/csr.h>
#if defined(CSR_SDRAM_GENERATOR_BASE) && defined(CSR_SDRAM_CHECKER_BASE)
#include <stdio.h>
#include <stdint.h>
#include <uart.h>
#include <time.h>
#include <console.h>

#include <generated/mem.h> /* MAIN_RAM_BASE, MAIN_RAM_SIZE */
#include <generated/soc.h> /* CONFIG_BIST_DATA_WIDTH */
#include <libbase/popc.h> /* popcount */
#include <libbase/memtest.h> /* memtest_inject_errors */
#include <liblitedram/bist.h>

/* un-comment to flip random bits in SDRAM */
// #define INJECT_ERRORS

/* un-comment to print detail about errors */
#define DISPLAY_ERRORS

#if !defined(MAIN_RAM_SIZE)
#define MAIN_RAM_SIZE 0x01000000
#endif

#define SDRAM_TEST_BASE 0x00000000
#define SDRAM_TEST_DATA_BYTES (CONFIG_BIST_DATA_WIDTH/8)
#define SDRAM_TEST_DATA_WORDS (SDRAM_TEST_DATA_BYTES/sizeof(uint32_t))

#if defined(CSR_SDRAM_ECCR_BASE)
#define SDRAM_TEST_SIZE MAIN_RAM_SIZE/2
#else
#define SDRAM_TEST_SIZE MAIN_RAM_SIZE
#endif

#define MAX_ERR_PRINT 10

static uint32_t wr_ticks;
static uint32_t wr_length;
static uint32_t rd_ticks;
static uint32_t rd_length;

static uint32_t rd_errors;
#if defined(CSR_SDRAM_ECCR_BASE)
static uint32_t sec_errors;
static uint32_t ded_errors;
#endif

static int wr_once = 2; /* if 1, BIST will only write once to SDRAM */
static uint32_t base;
static uint32_t rand_cnt; /* used for random address table lookup */

__attribute__((unused)) static void cdelay(int i) {
#ifndef CONFIG_BIOS_NO_DELAYS
	while(i > 0) {
		__asm__ volatile(CONFIG_CPU_NOP);
		i--;
	}
#endif
}

/* 128 entries, range 0x0 - 0x3FF000 (4M-4K) */
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

#define RANDOM_ADDR (SDRAM_TEST_BASE + pseudo_random_bases[(rand_cnt++)%PRB_SIZE]*SDRAM_TEST_DATA_BYTES)

/* data_mode: 0=pattern, 1=inc, 2=random */
static unsigned char mtab[] = {4, 0, 1}; /* translate from mode to register bits */

/* base and length are in bytes. */
/* They will be truncated by the bist hardware to data word alignment */
static inline void sdram_generator_init(uint32_t base, uint32_t length, int dmode) {
	sdram_generator_reset_write(1);
	sdram_generator_reset_write(0);
	sdram_generator_base_write(base);
	sdram_generator_end_write(base + length);
	sdram_generator_length_write(length);
	sdram_generator_mode_write(mtab[dmode]);
	cdelay(100);
}

static inline void sdram_checker_init(uint32_t base, uint32_t length, int dmode) {
#if defined(CSR_SDRAM_ECCR_BASE)
	sdram_eccr_clear_write(1); /* clears sdram_eccr_sec_errors & sdram_eccr_ded_errors */
	sdram_eccr_clear_write(0);
#endif
	sdram_checker_reset_write(1); /* clears sdram_checker_errors */
	sdram_checker_reset_write(0);
	sdram_checker_base_write(base);
	sdram_checker_end_write(base + length);
	sdram_checker_length_write(length);
	sdram_checker_mode_write(mtab[dmode]);
	cdelay(100);
}

/* for bandwidth calculations */
static void sdram_clear_counts(void) {
	wr_length = 0;
	wr_ticks  = 0;
	rd_length = 0;
	rd_ticks  = 0;
}

static void sdram_clear_errors(void) {
#if defined(CSR_SDRAM_ECCR_BASE)
	sec_errors = 0;
	ded_errors = 0;
#endif
	rd_errors = 0;
}

static uint32_t compute_speed_mibs(uint32_t length, uint32_t ticks) {
	if (ticks == 0) return 0;
	return (uint64_t)length*(CONFIG_CLOCK_FREQUENCY/(1024*1024))/ticks;
}

static void display_errors(uint32_t base, uint32_t length, int dmode) {
	uint32_t *ptr = (uint32_t *)(MAIN_RAM_BASE+base);
	uint32_t *end = (uint32_t *)(MAIN_RAM_BASE+base+length);
	uint32_t errors = 0;
	/* Flush caches */
	flush_cpu_dcache();
	flush_l2_cache();
	switch (dmode) {
	default:
	case 0: {
		uint32_t val = sdram_generator_pattern_read() * 0x01010101U; /* expand byte to word */
		do {
			if (*ptr++ != val && errors++ < MAX_ERR_PRINT)
				printf("error addr: 0x%08lx, content: 0x%08lx, expected: 0x%08lx\n",
					(unsigned long)(ptr - 1), *(ptr - 1), val);
		} while (ptr < end);
		break;}
	case 1: {
		uint32_t i = 0, cnt = 0;
		do {
			uint32_t val = ((i++ % SDRAM_TEST_DATA_WORDS) ? 0 : cnt++);
			if (*ptr++ != val && errors++ < MAX_ERR_PRINT)
				printf("error addr: 0x%08lx, content: 0x%08lx, expected: 0x%08lx\n",
					(unsigned long)(ptr - 1), *(ptr - 1), val);
		} while (ptr < end);
		break;}
	case 2:
		/* not implemented */
		break;
	}
	printf("ERRORS (32-bit words): %lu\n", errors);
}

#define TARGET_BYTES 0x20000
/* One DMA transaction is about 468 cycles */
/* For a burst > 0x400 trans., each DMA transaction is about 1 cycle per word */

static void sdram_bist_loop(uint32_t length, int amode, int dmode) {
	uint32_t next_base;
	uint32_t errors;
#if defined(CSR_SDRAM_ECCR_BASE)
	uint32_t sec, ded;
#endif

	/* Prepare first write */
	if (wr_once)
		sdram_generator_init(base, length, dmode);
	do {
		if (wr_once) {
			/* Start write */
			sdram_generator_start_write(1);
			wr_length += length;
		}
		/* Prepare next read */
		sdram_checker_init(base, length, dmode);
		if (wr_once) {
			/* Wait write */
			while (sdram_generator_done_read() == 0);
			wr_ticks += sdram_generator_ticks_read();
		}
		/* Start read */
		sdram_checker_start_write(1);
		rd_length += length;
		/* Calculate next address */
		switch (amode) {
		default:
		case MD_FIX:
			next_base = base;
			wr_once &= -2; /* Clear LSB, e.g. 1 goes to 0, 2 no change */
			break;
		case MD_INC:
			next_base = base + length;
			if (next_base >= (SDRAM_TEST_BASE+SDRAM_TEST_SIZE)) {
				next_base = SDRAM_TEST_BASE;
				wr_once &= -2;
			}
			break;
		case MD_RAN:
			next_base = RANDOM_ADDR;
			/* Write once mode not compatible with random address */
			/* mode when DMA blocks overlap. */
			if (rand_cnt == PRB_SIZE+1) wr_once &= -2;
			break;
		}
		/* Prepare next write */
		if (wr_once && rd_length+wr_length < TARGET_BYTES)
			sdram_generator_init(next_base, length, dmode);
		/* Wait read */
		while (sdram_checker_done_read() == 0);
		rd_ticks += sdram_checker_ticks_read();
		/* Report any errors, then fix them */
		rd_errors += errors = sdram_checker_errors_read();
#if defined(CSR_SDRAM_ECCR_BASE)
		sec_errors += sec = sdram_eccr_sec_errors_read();
		ded_errors += ded = sdram_eccr_ded_errors_read();
		errors += sec + ded;
#endif
		if (errors) {
#if defined(DISPLAY_ERRORS) && !defined(CSR_SDRAM_ECCR_BASE)
			display_errors(base, length, dmode);
#endif
			/* Re-write memory block to fix errors */
			sdram_bist_gen(base, length, dmode);
			/* Re-setup next memory block */
			if (wr_once && rd_length+wr_length < TARGET_BYTES)
				sdram_generator_init(next_base, length, dmode);
		}
		base = next_base;
	} while (rd_length+wr_length < TARGET_BYTES);
}

void sdram_bist_pattern(uint32_t value) {
	sdram_generator_pattern_write(value);
	sdram_checker_pattern_write(value);
}

void sdram_bist_gen(uint32_t base, uint32_t length, int dmode) {
	sdram_generator_init(base, length, dmode);
	sdram_generator_start_write(1);
	while (sdram_generator_done_read() == 0);
}

void sdram_bist_chk(uint32_t base, uint32_t length, int dmode) {
	uint32_t errors;
#if defined(CSR_SDRAM_ECCR_BASE)
	uint32_t sec, ded;
#endif

	sdram_checker_init(base, length, dmode);
	sdram_checker_start_write(1);
	while (sdram_checker_done_read() == 0);

	errors = sdram_checker_errors_read();
#if defined(CSR_SDRAM_ECCR_BASE)
	sec = sdram_eccr_sec_errors_read();
	ded = sdram_eccr_ded_errors_read();
	errors += sec + ded;
#endif

	if (errors) {
#if defined(DISPLAY_ERRORS) && !defined(CSR_SDRAM_ECCR_BASE)
		display_errors(base, length, dmode);
#endif
	} else printf("ERRORS: 0\n");
}

void sdram_bist(uint32_t length, int amode, int dmode, int wmode) {
	uint32_t i;
	uint32_t uc; /* update count */
	uint32_t uf; /* update frequency */
	uint32_t total_length;

	if (wmode < 2 && amode == MD_RAN) {
		printf("Write once mode not compatible with random address mode");
		return;
	}
	if (length < SDRAM_TEST_DATA_BYTES)
		length = SDRAM_TEST_DATA_BYTES;
	if (popcount(length) != 1) {
		printf("Value of length must be a power of 2");
		return;
	}

	printf("Starting SDRAM BIST with length=%lu, addr_mode=%d, data_mode=%d wmode=%d\n",
		length, amode, dmode, wmode);
	sdram_clear_counts();
	sdram_clear_errors();
	wr_once = wmode;
	i = 0;
	uc = 0;
	uf = length * 3 + 2;
	total_length = 0;
	rand_cnt = 0;
	if (amode == MD_RAN) base = RANDOM_ADDR;
	else base = SDRAM_TEST_BASE;
	for (;;) {
		/* Exit on key pressed */
		if (readchar_nonblock())
			break;

		/* Header */
		if (uc%8 == 0 && rd_length == 0) {
#if defined(CSR_SDRAM_ECCR_BASE)
			printf("WR-BW(MiB/s) RD-BW(MiB/s)  TESTED(MiB)     ERRORS        SEC        DED\n");
#else
			printf("WR-BW(MiB/s) RD-BW(MiB/s)  TESTED(MiB)     ERRORS\n");
#endif
		}

		/* BIST loop */
		sdram_bist_loop(length, amode, dmode);

		i++;
		/* Results, update about once per second */
		if ((length < 0x400 && i%uf == 0) ||
			(rd_ticks+wr_ticks >= CONFIG_CLOCK_FREQUENCY)) {
			uc++;
			total_length += rd_length;

#if defined(CSR_SDRAM_ECCR_BASE)
			printf("%12lu %12lu %12lu %10lu %10lu %10lu\n",
				compute_speed_mibs(wr_length, wr_ticks),
				compute_speed_mibs(rd_length, rd_ticks),
				total_length/(1024*1024),
				rd_errors, sec_errors, ded_errors);
#else
			printf("%12lu %12lu %12lu %10lu\n",
				compute_speed_mibs(wr_length, wr_ticks),
				compute_speed_mibs(rd_length, rd_ticks),
				total_length/(1024*1024),
				rd_errors);
#endif

			/* Clear bandwidth counts */
			sdram_clear_counts();

#if defined(INJECT_ERRORS)
			/* Inject SDRAM errors */
			if (wr_once < 2) {
				memtest_inject_errors((unsigned *)MAIN_RAM_BASE, MAIN_RAM_SIZE, 1, 0x1, 1);
			}
#endif /* INJECT_ERRORS */
		}
	}
}

#endif /* defined(CSR_SDRAM_GENERATOR_BASE) && defined(CSR_SDRAM_CHECKER_BASE) */
