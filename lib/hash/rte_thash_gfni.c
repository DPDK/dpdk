/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <stdbool.h>

#include <rte_log.h>
#include <rte_thash_gfni.h>

#ifndef RTE_THASH_GFNI_DEFINED

RTE_LOG_REGISTER_SUFFIX(hash_gfni_logtype, gfni, INFO);
#define RTE_LOGTYPE_HASH hash_gfni_logtype
#define HASH_LOG(level, ...) \
	RTE_LOG_LINE(level, HASH, "" __VA_ARGS__)

uint32_t
rte_thash_gfni(const uint64_t *mtrx __rte_unused,
	const uint8_t *key __rte_unused, int len __rte_unused)
{
	static bool warned;

	if (!warned) {
		warned = true;
		HASH_LOG(ERR,
			"%s is undefined under given arch", __func__);
	}

	return 0;
}

void
rte_thash_gfni_bulk(const uint64_t *mtrx __rte_unused,
	int len __rte_unused, uint8_t *tuple[] __rte_unused,
	uint32_t val[], uint32_t num)
{
	unsigned int i;

	static bool warned;

	if (!warned) {
		warned = true;
		HASH_LOG(ERR,
			"%s is undefined under given arch", __func__);
	}

	for (i = 0; i < num; i++)
		val[i] = 0;
}

#endif
