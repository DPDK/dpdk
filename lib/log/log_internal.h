/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#include <stdio.h>
#include <stdint.h>

#include <rte_compat.h>

/*
 * Initialize the default log stream.
 */
__rte_internal
int eal_log_init(const char *id, int facility);

/*
 * Determine where log data is written when no call to rte_openlog_stream.
 */
__rte_internal
void eal_log_set_default(FILE *default_log);

/*
 * Save a log option for later.
 */
__rte_internal
int eal_log_save_regexp(const char *regexp, uint32_t level);
__rte_internal
int eal_log_save_pattern(const char *pattern, uint32_t level);

/*
 * Convert log level to string.
 */
__rte_internal
const char *eal_log_level2str(uint32_t level);

/*
 * Close the default log stream
 */
__rte_internal
void rte_eal_log_cleanup(void);

#endif /* LOG_INTERNAL_H */
