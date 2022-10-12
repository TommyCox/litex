// SPDX-License-Identifier: BSD-Source-Code

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <generated/csr.h>
#include <generated/mem.h>
#include <generated/soc.h>

#include <libbase/i2c.h>
#include <libbase/memtest.h>

#include <liblitedram/sdram.h>
#include <liblitedram/bist.h>

#include "../command.h"
#include "../helpers.h"

/**
 * Command "sdram_init"
 *
 * Initialize SDRAM (Init + Calibration)
 *
 */
#if defined(CSR_SDRAM_BASE)
define_command(sdram_init, sdram_init, "Initialize SDRAM (Init + Calibration)", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_cal"
 *
 * Calibrate SDRAM
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_cal_handler(int nb_params, char **params)
{
	sdram_software_control_on();
	sdram_leveling();
	sdram_software_control_off();
}
define_command(sdram_cal, sdram_cal_handler, "Calibrate SDRAM", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_bitslip_scrub"
 * 
 * Set SDRAM bitslip to last known good configuration.
 * 
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_scrub_bitslip_handler(int nb_params, char **params)
{
	sdram_bitslip_scrub();
}
define_command(sdram_bitslip_scrub, sdram_scrub_bitslip_handler, "Set SDRAM bitslip to last known good configuration.", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_bitslip_set"
 * 
 * Set SDRAM bitslip.
 * 
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_set_bitslip_handler(int nb_params, char **params)
{
	char *c;
	int module, bitslip;

	if (nb_params < 2) {
		printf("sdram_bitslip_set <module> <bitslip>\n");
		return;
	}

	module = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Invalid module.\n");
	}
	bitslip = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Invalid bitslip.\n");
	}
	sdram_bitslip_set(module, bitslip);
}
define_command(sdram_bitslip_set, sdram_set_bitslip_handler, "Set SDRAM bitslip.", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_delay_scrub"
 * 
 * Set SDRAM delay to last known good configuration.
 * 
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_scrub_delay_handler(int nb_params, char **params)
{
	sdram_delay_scrub();
}
define_command(sdram_delay_scrub, sdram_scrub_delay_handler, "Set SDRAM delay to last known good configuration.", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_delay_set"
 * 
 * Set SDRAM delay.
 * 
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_set_delay_handler(int nb_params, char **params)
{
	char *c;
	int module, delay;

	if (nb_params < 2) {
		printf("sdram_delay_set <module> <delay>\n");
		return;
	}

	module = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Invalid module.\n");
	}
	delay = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Invalid delay.\n");
	}
	sdram_delay_set(module, delay);
}
define_command(sdram_delay_set, sdram_set_delay_handler, "Set SDRAM delay.", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_test"
 *
 * Test SDRAM
 *
 */
#if defined(CSR_SDRAM_BASE)
static void sdram_test_handler(int nb_params, char **params)
{
	memtest((unsigned int *)MAIN_RAM_BASE, MAIN_RAM_SIZE);
}
define_command(sdram_test, sdram_test_handler, "Test SDRAM", LITEDRAM_CMDS);
#endif

#if defined(CSR_SDRAM_GENERATOR_BASE) && defined(CSR_SDRAM_CHECKER_BASE)
/**
 * Command "sdram_bist_pat"
 *
 * Set SDRAM Build-In Self-Test (BIST) Pattern
 *
 */
static void sdram_pat_handler(int nb_params, char **params)
{
	char *c;
	uint32_t value;

	if (nb_params < 1) {
		printf("sdram_bist_pat <value>");
		return;
	}
	value = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect burst_length");
		return;
	}
	sdram_bist_pattern(value);
}
define_command(sdram_bist_pat, sdram_pat_handler, "Set SDRAM Build-In Self-Test Pattern", LITEDRAM_CMDS);

/**
 * Command "sdram_bist_gen"
 *
 * Run SDRAM Build-In Self-Test (BIST) Generator
 *
 */
static void sdram_gen_handler(int nb_params, char **params)
{
	char *c;
	uint32_t base;
	uint32_t length;
	int dmode = 2; /* default: random data*/

	if (nb_params < 2) {
		printf("sdram_bist_gen <base> <length> [<data_mode>]\n");
		printf("base     : base address (starts at zero)\n");
		printf("length   : DMA block size in bytes\n");
		printf("data_mode: 0=pattern, 1=inc, 2=random");
		return;
	}
	base = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect base");
		return;
	}
	length = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect length");
		return;
	}
	if (nb_params > 2) {
		dmode = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect data_mode");
			return;
		}
	}
	sdram_bist_gen(base, length, dmode);
}
define_command(sdram_bist_gen, sdram_gen_handler, "Run SDRAM Build-In Self-Test Generator", LITEDRAM_CMDS);

/**
 * Command "sdram_bist_chk"
 *
 * Run SDRAM Build-In Self-Test (BIST) Checker
 *
 */
static void sdram_chk_handler(int nb_params, char **params)
{
	char *c;
	uint32_t base;
	uint32_t length;
	int dmode = 2; /* default: random data*/

	if (nb_params < 2) {
		printf("sdram_bist_chk <base> <length> [<data_mode>]\n");
		printf("base     : base address (starts at zero)\n");
		printf("length   : DMA block size in bytes\n");
		printf("data_mode: 0=pattern, 1=inc, 2=random");
		return;
	}
	base = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect base");
		return;
	}
	length = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect length");
		return;
	}
	if (nb_params > 2) {
		dmode = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect data_mode");
			return;
		}
	}
	sdram_bist_chk(base, length, dmode);
}
define_command(sdram_bist_chk, sdram_chk_handler, "Run SDRAM Build-In Self-Test Checker", LITEDRAM_CMDS);

/**
 * Command "sdram_bist"
 *
 * Run SDRAM Build-In Self-Test (BIST)
 *
 */
static void sdram_bist_handler(int nb_params, char **params)
{
	char *c;
	uint32_t length;
	int amode = 1; /* default: increment */
	int dmode = 2; /* default: random data*/
	int wmode = 2; /* default: write before each read */

	if (nb_params < 1) {
		printf("sdram_bist <length> [<addr_mode>] [<data_mode>] [<write_mode>]\n");
		printf("length    : DMA block size in bytes\n");
		printf("addr_mode : 0=fixed (starts at zero), 1=inc, 2=random\n");
		printf("data_mode : 0=pattern, 1=inc, 2=random\n");
		printf("write_mode: 0=no_write, 1=write_once, 2=write_and_read");
		return;
	}
	length = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect length");
		return;
	}
	if (nb_params > 1) {
		amode = strtoul(params[1], &c, 0);
		if (*c != 0) {
			printf("Incorrect addr_mode");
			return;
		}
	}
	if (nb_params > 2) {
		dmode = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect data_mode");
			return;
		}
	}
	if (nb_params > 3) {
		wmode = strtoul(params[3], &c, 0);
		if (*c != 0) {
			printf("Incorrect write_once");
			return;
		}
	}
	sdram_bist(length, amode, dmode, wmode);
}
define_command(sdram_bist, sdram_bist_handler, "Run SDRAM Build-In Self-Test", LITEDRAM_CMDS);
#endif /* defined(CSR_SDRAM_GENERATOR_BASE) && defined(CSR_SDRAM_CHECKER_BASE) */

#ifdef CSR_DDRPHY_RDPHASE_ADDR
/**
 * Command "sdram_force_rdphase"
 *
 * Force read phase
 *
 */
static void sdram_force_rdphase_handler(int nb_params, char **params)
{
	char *c;
	int phase;
	if (nb_params < 1) {
		printf("sdram_force_rdphase <phase>");
		return;
	}
	phase = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect phase");
		return;
	}
	printf("Forcing read phase to %d\n", phase);
	ddrphy_rdphase_write(phase);
}
define_command(sdram_force_rdphase, sdram_force_rdphase_handler, "Force read phase", LITEDRAM_CMDS);
#endif

#ifdef CSR_DDRPHY_WRPHASE_ADDR
/**
 * Command "sdram_force_wrphase"
 *
 * Force write phase
 *
 */
static void sdram_force_wrphase_handler(int nb_params, char **params)
{
	char *c;
	int phase;
	if (nb_params < 1) {
		printf("sdram_force_wrphase <phase>");
		return;
	}
	phase = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect phase");
		return;
	}
	printf("Forcing write phase to %d\n", phase);
	ddrphy_wrphase_write(phase);
}
define_command(sdram_force_wrphase, sdram_force_wrphase_handler, "Force write phase", LITEDRAM_CMDS);
#endif

#ifdef CSR_DDRPHY_CDLY_RST_ADDR

/**
 * Command "sdram_rst_cmd_delay"
 *
 * Reset write leveling Cmd delay
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_rst_cmd_delay_handler(int nb_params, char **params)
{
	sdram_software_control_on();
	sdram_write_leveling_rst_cmd_delay(1);
	sdram_software_control_off();
}
define_command(sdram_rst_cmd_delay, sdram_rst_cmd_delay_handler, "Reset write leveling Cmd delay", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_force_cmd_delay"
 *
 * Force write leveling Cmd delay
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_force_cmd_delay_handler(int nb_params, char **params)
{
	char *c;
	int taps;
	if (nb_params < 1) {
		printf("sdram_force_cmd_delay <taps>");
		return;
	}
	taps = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect taps");
		return;
	}
	sdram_software_control_on();
	sdram_write_leveling_force_cmd_delay(taps, 1);
	sdram_software_control_off();
}
define_command(sdram_force_cmd_delay, sdram_force_cmd_delay_handler, "Force write leveling Cmd delay", LITEDRAM_CMDS);
#endif

#endif

#ifdef CSR_DDRPHY_WDLY_DQ_RST_ADDR

/**
 * Command "sdram_rst_dat_delay"
 *
 * Reset write leveling Dat delay
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_rst_dat_delay_handler(int nb_params, char **params)
{
	char *c;
	int module;
	if (nb_params < 1) {
		printf("sdram_rst_dat_delay <module>");
		return;
	}
	module = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect module");
		return;
	}
	sdram_software_control_on();
	sdram_write_leveling_rst_dat_delay(module, 1);
	sdram_software_control_off();
}
define_command(sdram_rst_dat_delay, sdram_rst_dat_delay_handler, "Reset write leveling Dat delay", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_force_dat_delay"
 *
 * Force write leveling Dat delay
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_force_dat_delay_handler(int nb_params, char **params)
{
	char *c;
	int module;
	int taps;
	if (nb_params < 2) {
		printf("sdram_force_dat_delay <module> <taps>");
		return;
	}
	module = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect module");
		return;
	}
	taps = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect taps");
		return;
	}
	sdram_software_control_on();
	sdram_write_leveling_force_dat_delay(module, taps, 1);
	sdram_software_control_off();
}
define_command(sdram_force_dat_delay, sdram_force_dat_delay_handler, "Force write leveling Dat delay", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_rst_bitslip"
 *
 * Reset write leveling Bitslip
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_rst_bitslip_handler(int nb_params, char **params)
{
	char *c;
	int module;
	if (nb_params < 1) {
		printf("sdram_rst_bitslip <module>");
		return;
	}
	module = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect module");
		return;
	}
	sdram_software_control_on();
	sdram_write_leveling_rst_bitslip(module, 1);
	sdram_software_control_off();
}
define_command(sdram_rst_bitslip, sdram_rst_bitslip_handler, "Reset write leveling Bitslip", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_force_bitslip"
 *
 * Force write leveling Bitslip
 *
 */
#if defined(CSR_SDRAM_BASE) && defined(CSR_DDRPHY_BASE)
static void sdram_force_bitslip_handler(int nb_params, char **params)
{
	char *c;
	int module;
	int bitslip;
	if (nb_params < 2) {
		printf("sdram_force_bitslip <module> <bitslip>");
		return;
	}
	module = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect module");
		return;
	}
	bitslip = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect bitslip");
		return;
	}
	sdram_software_control_on();
	sdram_write_leveling_force_bitslip(module, bitslip, 1);
	sdram_software_control_off();
}
define_command(sdram_force_bitslip, sdram_force_bitslip_handler, "Force write leveling Bitslip", LITEDRAM_CMDS);
#endif

#endif

/**
 * Command "sdram_mr_write"
 *
 * Write SDRAM Mode Register
 *
 */
#if defined(CSR_SDRAM_BASE)
static void sdram_mr_write_handler(int nb_params, char **params)
{
	char *c;
	uint8_t reg;
	uint16_t value;

	if (nb_params < 2) {
		printf("sdram_mr_write <reg> <value>");
		return;
	}
	reg = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect reg");
		return;
	}
	value = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect value");
		return;
	}
	sdram_software_control_on();
	printf("Writing 0x%04x to MR%d\n", value, reg);
	sdram_mode_register_write(reg, value);
	sdram_software_control_off();
}
define_command(sdram_mr_write, sdram_mr_write_handler, "Write SDRAM Mode Register", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_mr_scrub"
 *
 * Reset SDRAM Mode Register to defaults.
 *
 */
#if defined(CSR_SDRAM_BASE)
static void sdram_mr_scrub_handler(int nb_params, char **params)
{
	sdram_software_control_on();
	sdram_mode_register_scrub();
	sdram_software_control_off();
}
define_command(sdram_mr_scrub, sdram_mr_scrub_handler, "Reset SDRAM Mode Register to defaults.", LITEDRAM_CMDS);
#endif

/**
 * Command "sdram_spd"
 *
 * Read contents of SPD EEPROM memory.
 * SPD address is a 3-bit address defined by the pins A0, A1, A2.
 *
 */
#ifdef CONFIG_HAS_I2C
#define SPD_RW_PREAMBLE    0b1010
#define SPD_RW_ADDR(a210)  ((SPD_RW_PREAMBLE << 3) | ((a210) & 0b111))

static void sdram_spd_handler(int nb_params, char **params)
{
	char *c;
	unsigned char spdaddr;
	unsigned char buf[256];
	int len = sizeof(buf);
	bool send_stop = true;

	if (nb_params < 1) {
		printf("sdram_spd <spdaddr> [<send_stop>]");
		return;
	}

	spdaddr = strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}
	if (spdaddr > 0b111) {
		printf("SPD EEPROM max address is 0b111 (defined by A0, A1, A2 pins)");
		return;
	}

	if (nb_params > 1) {
		send_stop = strtoul(params[1], &c, 0) != 0;
		if (*c != 0) {
			printf("Incorrect send_stop value");
			return;
		}
	}

	if (!i2c_read(SPD_RW_ADDR(spdaddr), 0, buf, len, send_stop)) {
		printf("Error when reading SPD EEPROM");
		return;
	}

	dump_bytes((unsigned int *) buf, len, 0);

#ifdef SPD_BASE
	{
		int cmp_result;
		cmp_result = memcmp(buf, (void *) SPD_BASE, SPD_SIZE);
		if (cmp_result == 0) {
			printf("Memory conents matches the data used for gateware generation\n");
		} else {
			printf("\nWARNING: memory differs from the data used during gateware generation:\n");
			dump_bytes((void *) SPD_BASE, SPD_SIZE, 0);
		}
	}
#endif
}
define_command(sdram_spd, sdram_spd_handler, "Read SDRAM SPD EEPROM", LITEDRAM_CMDS);
#endif

#ifdef SDRAM_DEBUG
define_command(sdram_debug, sdram_debug, "Run SDRAM debug tests", LITEDRAM_CMDS);
#endif
