/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include "eal_private.h"

/* set the log to default function, called during eal init process. */
int
rte_eal_log_init(__rte_unused const char *id, __rte_unused int facility)
{
	rte_openlog_stream(stderr);

	eal_log_set_default(stderr);

	return 0;
}
