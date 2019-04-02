/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_common.h>

 /* call abort(), it will generate a coredump if enabled */
void
__rte_panic(const char *funcname __rte_unused,
		const char *format __rte_unused, ...)
{
	/* TODO */
	/* This is a stub, not the expected result */
	abort();
}
