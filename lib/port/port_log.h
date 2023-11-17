/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>

#define PORT_LOG(level, ...) \
	RTE_LOG_LINE(level, PORT, "" __VA_ARGS__)

