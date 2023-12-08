/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#include <string.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_mbuf_pool_ops.h>

#include "mbuf_log.h"

int
rte_mbuf_set_platform_mempool_ops(const char *ops_name)
{
	const struct rte_memzone *mz;

	size_t len = strnlen(ops_name, RTE_MEMPOOL_OPS_NAMESIZE);
	if (len == 0)
		return -EINVAL;
	if (len == RTE_MEMPOOL_OPS_NAMESIZE)
		return -ENAMETOOLONG;

	mz = rte_memzone_lookup("mbuf_platform_pool_ops");
	if (mz == NULL) {
		mz = rte_memzone_reserve("mbuf_platform_pool_ops",
			RTE_MEMPOOL_OPS_NAMESIZE, SOCKET_ID_ANY, 0);
		if (mz == NULL)
			return -rte_errno;
		strcpy(mz->addr, ops_name);
		return 0;
	} else if (strcmp(mz->addr, ops_name) == 0) {
		return 0;
	}

	MBUF_LOG(ERR,
		"%s is already registered as platform mbuf pool ops",
		(char *)mz->addr);
	return -EEXIST;
}

const char *
rte_mbuf_platform_mempool_ops(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup("mbuf_platform_pool_ops");
	if (mz == NULL)
		return NULL;
	return mz->addr;
}

int
rte_mbuf_set_user_mempool_ops(const char *ops_name)
{
	const struct rte_memzone *mz;

	size_t len = strnlen(ops_name, RTE_MEMPOOL_OPS_NAMESIZE);
	if (len == 0)
		return -EINVAL;
	if (len == RTE_MEMPOOL_OPS_NAMESIZE)
		return -ENAMETOOLONG;

	mz = rte_memzone_lookup("mbuf_user_pool_ops");
	if (mz == NULL) {
		mz = rte_memzone_reserve("mbuf_user_pool_ops",
			RTE_MEMPOOL_OPS_NAMESIZE, SOCKET_ID_ANY, 0);
		if (mz == NULL)
			return -rte_errno;
	}

	strcpy(mz->addr, ops_name);
	return 0;

}

const char *
rte_mbuf_user_mempool_ops(void)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup("mbuf_user_pool_ops");
	if (mz == NULL)
		return rte_eal_mbuf_user_pool_ops();
	return mz->addr;
}

/* Return mbuf pool ops name */
const char *
rte_mbuf_best_mempool_ops(void)
{
	/* User defined mempool ops takes the priority */
	const char *best_ops = rte_mbuf_user_mempool_ops();
	if (best_ops)
		return best_ops;

	/* Next choice is platform configured mempool ops */
	best_ops = rte_mbuf_platform_mempool_ops();
	if (best_ops)
		return best_ops;

	/* Last choice is to use the compile time config pool */
	return RTE_MBUF_DEFAULT_MEMPOOL_OPS;
}
