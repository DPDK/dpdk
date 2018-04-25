/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

extern int tap_logtype;

#define TAP_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, tap_logtype, "%s(): " fmt "\n", \
		__func__, ## args)
