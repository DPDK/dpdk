/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_errno.h>
#include <rte_reorder.h>

#include "pdcp_reorder.h"

int
pdcp_reorder_create(struct pdcp_reorder *reorder, uint32_t window_size)
{
	reorder->buf = rte_reorder_create("reorder_buffer", SOCKET_ID_ANY, window_size);
	if (reorder->buf == NULL)
		return -rte_errno;

	reorder->window_size = window_size;
	reorder->is_active = false;

	return 0;
}

void
pdcp_reorder_destroy(const struct pdcp_reorder *reorder)
{
	rte_reorder_free(reorder->buf);
}
