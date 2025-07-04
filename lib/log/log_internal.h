/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include <rte_compat.h>

/*
 * Initialize the default log stream.
 */
__rte_internal
void eal_log_init(const char *id);

/*
 * Save a log option for later.
 */
__rte_internal
int eal_log_save_regexp(const char *regexp, uint32_t level);
__rte_internal
int eal_log_save_pattern(const char *pattern, uint32_t level);

__rte_internal
int eal_log_syslog(const char *name);

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

/*
 * Add timestamp to console logs
 */
__rte_internal
int eal_log_timestamp(const char *fmt);

/*
 * Enable or disable color in log messages
 */
__rte_internal
int eal_log_color(const char *mode);

#endif /* LOG_INTERNAL_H */
