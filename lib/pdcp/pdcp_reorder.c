/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_errno.h>
#include <rte_reorder.h>

#include "pdcp_reorder.h"

int
pdcp_reorder_create(struct pdcp_reorder *reorder, size_t nb_elem, void *mem, size_t mem_size)
{
	reorder->buf = rte_reorder_init(mem, mem_size, "reorder_buffer", nb_elem);
	if (reorder->buf == NULL)
		return -rte_errno;

	reorder->is_active = false;

	return 0;
}
