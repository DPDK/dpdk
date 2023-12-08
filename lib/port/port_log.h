/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>

#define PORT_LOG(level, fmt, ...) \
	RTE_LOG(level, PORT, fmt "\n", ## __VA_ARGS__)

