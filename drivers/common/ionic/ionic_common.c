/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2021 Pensando Systems, Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_common.h>

#include "ionic_common.h"

#if 0
// Added in DPDK 20.08
size_t
rte_mem_page_size(void)
{
	static size_t page_size;

	if (!page_size)
		page_size = sysconf(_SC_PAGESIZE);

	return page_size;
}
#endif
