/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_spinlock.h>

#include "graph_private.h"

static rte_spinlock_t graph_lock = RTE_SPINLOCK_INITIALIZER;

void
graph_spinlock_lock(void)
{
	rte_spinlock_lock(&graph_lock);
}

void
graph_spinlock_unlock(void)
{
	rte_spinlock_unlock(&graph_lock);
}
