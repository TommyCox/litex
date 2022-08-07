// SPDX-License-Identifier: BSD-Source-Code

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libbase/memtest.h>

#include <generated/csr.h>
#include <generated/mem.h>

#include "../command.h"
#include "../helpers.h"

#define MAX_ERR_PRINT 10

/**
 * Command "mem_list"
 *
 * Memory list
 *
 */
static void mem_list_handler(int nb_params, char **params)
{
	printf("Available memory regions:\n");
	puts(MEM_REGIONS);
}
define_command(mem_list, mem_list_handler, "List available memory regions", MEM_CMDS);

/**
 * Command "mem_read"
 *
 * Memory read
 *
 */
static void mem_read_handler(int nb_params, char **params)
{
	char *c;
	unsigned int *addr;
	unsigned int length = 4;

	if (nb_params < 1) {
		printf("mem_read <address> [<length>]");
		return;
	}
	addr = (unsigned int *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}
	if (nb_params > 1) {
		length = strtoul(params[1], &c, 0);
		if (*c != 0) {
			printf("Incorrect length");
			return;
		}
	}

	dump_bytes(addr, length, (unsigned long)addr);
}
define_command(mem_read, mem_read_handler, "Read address space", MEM_CMDS);

/**
 * Command "mem_write"
 *
 * Memory write
 *
 */
static void mem_write_handler(int nb_params, char **params)
{
	char *c;
	void *addr;
	unsigned int value;
	unsigned int count = 1;
	unsigned int size = 4;

	if (nb_params < 2) {
		printf("mem_write <address> <value> [<count>] [<size>]");
		return;
	}

	addr = (void *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}

	value = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect value");
		return;
	}

	if (nb_params > 2) {
		count = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect count");
			return;
		}
	}

	if (nb_params > 3) {
		size = strtoul(params[3], &c, 0);
		if (*c != 0) {
			printf("Incorrect size");
			return;
		}
	}

	switch (size) {
	case 1: {
		uint8_t *ptr = addr, *end = ptr+count;
		do *ptr++ = value; while (ptr < end);
		break;}
	case 2: {
		uint16_t *ptr = addr, *end = ptr+count;
		do *ptr++ = value; while (ptr < end);
		break;}
	case 4: {
		uint32_t *ptr = addr, *end = ptr+count;
		do *ptr++ = value; while (ptr < end);
		break;}
	default:
		printf("Only a size of 1, 2, or 4 is supported");
		return;
	}
}
define_command(mem_write, mem_write_handler, "Write address space", MEM_CMDS);

/**
 * Command "mem_check"
 *
 * Memory check
 *
 */
static void mem_check_handler(int nb_params, char **params)
{
	char *c;
	void *addr;
	unsigned int value;
	unsigned int count = 1;
	unsigned int size = 4;
	uint32_t errors = 0;

	if (nb_params < 2) {
		printf("mem_check <address> <value> [<count>] [<size>]");
		return;
	}

	addr = (void *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}

	value = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect value");
		return;
	}

	if (nb_params > 2) {
		count = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect count");
			return;
		}
	}

	if (nb_params > 3) {
		size = strtoul(params[3], &c, 0);
		if (*c != 0) {
			printf("Incorrect size");
			return;
		}
	}

	switch (size) {
	case 1: {
		uint8_t *ptr = addr, *end = ptr+count, val = value;
		do {
			if (*ptr++ != val && errors++ < MAX_ERR_PRINT)
				printf("error addr: 0x%08lx, content: 0x%02x, expected: 0x%02x\n",
					(unsigned long)(ptr - 1), *(ptr - 1), val);
		} while (ptr < end);
		break;}
	case 2: {
		uint16_t *ptr = addr, *end = ptr+count, val = value;
		do {
			if (*ptr++ != val && errors++ < MAX_ERR_PRINT)
				printf("error addr: 0x%08lx, content: 0x%04x, expected: 0x%04x\n",
					(unsigned long)(ptr - 1), *(ptr - 1), val);
		} while (ptr < end);
		break;}
	case 4: {
		uint32_t *ptr = addr, *end = ptr+count, val = value;
		do {
			if (*ptr++ != val && errors++ < MAX_ERR_PRINT)
				printf("error addr: 0x%08lx, content: 0x%08lx, expected: 0x%08lx\n",
					(unsigned long)(ptr - 1), *(ptr - 1), val);
		} while (ptr < end);
		break;}
	default:
		printf("Only a size of 1, 2, or 4 is supported");
		return;
	}
	printf("ERRORS: %lu\n", errors);
}
define_command(mem_check, mem_check_handler, "Check address space", MEM_CMDS);

/**
 * Command "mem_copy"
 *
 * Memory copy
 *
 */
static void mem_copy_handler(int nb_params, char **params)
{
	char *c;
	unsigned int *dstaddr;
	unsigned int *srcaddr;
	unsigned int count = 1;
	unsigned int i;

	if (nb_params < 2) {
		printf("mem_copy <dst> <src> [<count>]");
		return;
	}

	dstaddr = (unsigned int *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect destination address");
		return;
	}

	srcaddr = (unsigned int *)strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect source address");
		return;
	}

	if (nb_params > 2) {
		count = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect count");
			return;
		}
	}

	for (i = 0; i < count; i++)
		*dstaddr++ = *srcaddr++;
}
define_command(mem_copy, mem_copy_handler, "Copy address space", MEM_CMDS);

/**
 * Command "mem_inject_err"
 *
 * Memory Inject Errors, flip random bits
 *
 */
static void mem_inject_handler(int nb_params, char **params)
{
	char *c;
	unsigned int *addr;
	unsigned long size;
	unsigned int errors = 1;

	if (nb_params < 2) {
		printf("mem_inject_err <address> <size> [<errors>]");
		return;
	}

	addr = (unsigned int *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}

	size = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect size");
		return;
	}

	if (nb_params > 2) {
		errors = strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect error count");
			return;
		}
	}

	memtest_inject_errors(addr, size, errors, 0x1, 1);
}
define_command(mem_inject_err, mem_inject_handler, "Inject errors in address space", MEM_CMDS);

/**
 * Command "mem_test"
 *
 * Memory Test
 *
 */
static void mem_test_handler(int nb_params, char **params)
{
	char *c;
	unsigned int *addr;
	unsigned long maxsize = ~0uL;

	if (nb_params < 1) {
		printf("mem_test <address> [<maxsize>]");
		return;
	}

	addr = (unsigned int *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}

	if (nb_params > 1) {
		maxsize = strtoul(params[1], &c, 0);
		if (*c != 0) {
			printf("Incorrect size");
			return;
		}

	}

	memtest(addr, maxsize);
}
define_command(mem_test, mem_test_handler, "Test memory access", MEM_CMDS);

/**
 * Command "mem_speed"
 *
 * Memory Speed
 *
 */
static void mem_speed_handler(int nb_params, char **params)
{
	char *c;
	unsigned int *addr;
	unsigned long size;
	bool read_only = false;
	bool random = false;

	if (nb_params < 2) {
		printf("mem_speed <address> <size> [<readonly>] [<random>]");
		return;
	}

	addr = (unsigned int *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect address");
		return;
	}

	size = strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect size");
		return;
	}

	if (nb_params > 2) {
		read_only = (bool)strtoul(params[2], &c, 0);
		if (*c != 0) {
			printf("Incorrect readonly value");
			return;
		}
	}

	if (nb_params > 3) {
		random = (bool)strtoul(params[3], &c, 0);
		if (*c != 0) {
			printf("Incorrect random value");
			return;
		}
	}

	memspeed(addr, size, read_only, random);
}
define_command(mem_speed, mem_speed_handler, "Test memory speed", MEM_CMDS);

/**
 * Command "mem_cmp"
 *
 * Memory Compare
 *
 */
static void mem_cmp_handler(int nb_params, char **params)
{
	char *c;
	unsigned int *addr1;
	unsigned int *addr2;
	unsigned int count;
	unsigned int i;
	bool same = true;

	if (nb_params < 3) {
		printf("mem_cmp <addr1> <addr2> <count>");
		return;
	}

	addr1 = (unsigned int *)strtoul(params[0], &c, 0);
	if (*c != 0) {
		printf("Incorrect addr1");
		return;
	}

	addr2 = (unsigned int *)strtoul(params[1], &c, 0);
	if (*c != 0) {
		printf("Incorrect addr2");
		return;
	}

	count = strtoul(params[2], &c, 0);
	if (*c != 0) {
		printf("Incorrect count");
		return;
	}

	for (i = 0; i < count; i++)
		if (*addr1++ != *addr2++){
			printf("Different memory content:\naddr1: 0x%08lx, content: 0x%08x\naddr2: 0x%08lx, content: 0x%08x\n",
				(long unsigned int)(addr1 - 1), *(addr1 - 1),
				(long unsigned int)(addr2 - 1), *(addr2 - 1));
			same = false;
		}

	if (same)
		printf("mem_cmp finished, same content.");
	else
		printf("mem_cmp finished, different content.");
}
define_command(mem_cmp, mem_cmp_handler, "Compare memory content", MEM_CMDS);
