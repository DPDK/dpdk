/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_memory.h>
#include <rte_common.h>
#include <rte_memzone.h>

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
check_mem(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg __rte_unused)
{
	volatile uint8_t *mem = (volatile uint8_t *) ms->addr;
	size_t i, max = ms->len;

	for (i = 0; i < max; i++, mem++)
		*mem;
	return 0;
}

static int
test_memory(void)
{
	uint64_t s;

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
	rte_memseg_walk(check_mem, NULL);

	return 0;
}

REGISTER_TEST_COMMAND(memory_autotest, test_memory);
