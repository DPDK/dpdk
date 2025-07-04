/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_eal.h>
#include <rte_errno.h>
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

/*
 * ASan complains about accessing unallocated memory.
 * See: https://bugs.dpdk.org/show_bug.cgi?id=880
 */
__rte_no_asan
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
check_seg_fds(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		void *arg __rte_unused)
{
	size_t offset;
	int ret;

	/* skip external segments */
	if (msl->external)
		return 0;

	/* try segment fd first. we're in a callback, so thread-unsafe */
	ret = rte_memseg_get_fd_thread_unsafe(ms);
	if (ret < 0) {
		/* ENOTSUP means segment is valid, but there is not support for
		 * segment fd API (e.g. on FreeBSD).
		 */
		if (rte_errno == ENOTSUP)
			return 1;
		/* all other errors are treated as failures */
		return -1;
	}

	/* we're able to get memseg fd - try getting its offset */
	ret = rte_memseg_get_fd_offset_thread_unsafe(ms, &offset);
	if (ret < 0) {
		if (rte_errno == ENOTSUP)
			return 1;
		return -1;
	}
	return 0;
}

static int
test_memory(void)
{
	uint64_t s;
	int ret;

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

	/* check segment fd support */
	ret = rte_memseg_walk(check_seg_fds, NULL);
	if (ret == 1) {
		printf("Segment fd API is unsupported\n");
	} else if (ret == -1) {
		printf("Error getting segment fd's\n");
		return -1;
	}

	return 0;
}

REGISTER_FAST_TEST(memory_autotest, false, true, test_memory);
