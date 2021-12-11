/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _CN9K_REGEXDEV_H_
#define _CN9K_REGEXDEV_H_

#include <rte_common.h>
#include <rte_regexdev.h>

#include "roc_api.h"

#define cn9k_ree_dbg plt_ree_dbg
#define cn9k_err plt_err

#define ree_func_trace cn9k_ree_dbg

/* Marvell CN9K Regex PMD device name */
#define REGEXDEV_NAME_CN9K_PMD	regex_cn9k

/**
 * Device private data
 */
struct cn9k_ree_data {
	uint32_t regexdev_capa;
	uint64_t rule_flags;
	/**< Feature flags exposes HW/SW features for the given device */
	uint16_t max_rules_per_group;
	/**< Maximum rules supported per subset by this device */
	uint16_t max_groups;
	/**< Maximum subset supported by this device */
	void **queue_pairs;
	/**< Array of pointers to queue pairs. */
	uint16_t nb_queue_pairs;
	/**< Number of device queue pairs. */
	struct roc_ree_vf vf;
	/**< vf data */
	struct rte_regexdev_rule *rules;
	/**< rules to be compiled */
	uint16_t nb_rules;
	/**< number of rules */
} __rte_cache_aligned;

#endif /* _CN9K_REGEXDEV_H_ */
