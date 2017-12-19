/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_memory.h>
#include <rte_common.h>

#include "test.h"

/*
 * Memory
 * ======
 *
 * - Dump the mapped memory. The python-expect script checks that at
 *   least one line is dumped.
 *
 * - Check that memory size is different than 0.
 *
 * - Try to read all memory; it should not segfault.
 */

static int
test_memory(void)
{
	uint64_t s;
	unsigned i;
	size_t j;
	const struct rte_memseg *mem;

	/*
	 * dump the mapped memory: the python-expect script checks
	 * that at least one line is dumped
	 */
	printf("Dump memory layout\n");
	rte_dump_physmem_layout(stdout);

	/* check that memory size is != 0 */
	s = rte_eal_get_physmem_size();
	if (s == 0) {
		printf("No memory detected\n");
		return -1;
	}

	/* try to read memory (should not segfault) */
	mem = rte_eal_get_physmem_layout();
	for (i = 0; i < RTE_MAX_MEMSEG && mem[i].addr != NULL ; i++) {

		/* check memory */
		for (j = 0; j<mem[i].len; j++) {
			*((volatile uint8_t *) mem[i].addr + j);
		}
	}

	return 0;
}

REGISTER_TEST_COMMAND(memory_autotest, test_memory);
