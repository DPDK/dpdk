/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_config.h>
#include <rte_eal_memconfig.h>

#include "eal_memcfg.h"

void
rte_mcfg_mem_read_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_lock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_mem_read_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_mem_write_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_lock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_mem_write_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_unlock(&mcfg->memory_hotplug_lock);
}

void
rte_mcfg_tailq_read_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_lock(&mcfg->qlock);
}

void
rte_mcfg_tailq_read_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_unlock(&mcfg->qlock);
}

void
rte_mcfg_tailq_write_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_lock(&mcfg->qlock);
}

void
rte_mcfg_tailq_write_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_unlock(&mcfg->qlock);
}

void
rte_mcfg_mempool_read_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_lock(&mcfg->mplock);
}

void
rte_mcfg_mempool_read_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_read_unlock(&mcfg->mplock);
}

void
rte_mcfg_mempool_write_lock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_lock(&mcfg->mplock);
}

void
rte_mcfg_mempool_write_unlock(void)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	rte_rwlock_write_unlock(&mcfg->mplock);
}
