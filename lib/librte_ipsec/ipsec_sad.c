/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_errno.h>

#include "rte_ipsec_sad.h"

int
rte_ipsec_sad_add(__rte_unused struct rte_ipsec_sad *sad,
		__rte_unused const union rte_ipsec_sad_key *key,
		__rte_unused int key_type, __rte_unused void *sa)
{
	return -ENOTSUP;
}

int
rte_ipsec_sad_del(__rte_unused struct rte_ipsec_sad *sad,
		__rte_unused const union rte_ipsec_sad_key *key,
		__rte_unused int key_type)
{
	return -ENOTSUP;
}

struct rte_ipsec_sad *
rte_ipsec_sad_create(__rte_unused const char *name,
		__rte_unused const struct rte_ipsec_sad_conf *conf)
{
	return NULL;
}

struct rte_ipsec_sad *
rte_ipsec_sad_find_existing(__rte_unused const char *name)
{
	return NULL;
}

void
rte_ipsec_sad_destroy(__rte_unused struct rte_ipsec_sad *sad)
{
	return;
}

int
rte_ipsec_sad_lookup(__rte_unused const struct rte_ipsec_sad *sad,
		__rte_unused const union rte_ipsec_sad_key *keys[],
		__rte_unused void *sa[], __rte_unused uint32_t n)
{
	return -ENOTSUP;
}
