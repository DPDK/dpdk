/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <stdlib.h>
#include <errno.h>

#include <rte_alarm.h>
#include <rte_common.h>
#include "eal_private.h"

int
rte_eal_alarm_init(void)
{
	return 0;
}


int
rte_eal_alarm_set(uint64_t us __rte_unused,
		rte_eal_alarm_callback cb_fn __rte_unused,
		void *cb_arg __rte_unused)
{
	return -ENOTSUP;
}

int
rte_eal_alarm_cancel(rte_eal_alarm_callback cb_fn __rte_unused,
		void *cb_arg __rte_unused)
{
	return -ENOTSUP;
}
