/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_telemetry.h>

#include <roc_api.h>

#include "cnxk_mempool.h"
#include "cnxk_telemetry.h"

struct mempool_info_cb_arg {
	char *pool_name;
	struct rte_tel_data *d;
};

static void
mempool_info_cb(struct rte_mempool *mp, void *arg)
{
	struct mempool_info_cb_arg *info = (struct mempool_info_cb_arg *)arg;
	int aura_id;

	if (strncmp(mp->name, info->pool_name, RTE_MEMZONE_NAMESIZE))
		return;

	aura_id = roc_npa_aura_handle_to_aura(mp->pool_id);
	rte_tel_data_add_dict_int(info->d, "aura_id", aura_id);
}

static int
mempool_tel_handle_info(const char *cmd __rte_unused, const char *params,
			struct rte_tel_data *d)
{
	struct mempool_info_cb_arg mp_arg;
	char name[RTE_MEMZONE_NAMESIZE];

	if (params == NULL || strlen(params) == 0)
		return -EINVAL;

	rte_strlcpy(name, params, RTE_MEMZONE_NAMESIZE);

	rte_tel_data_start_dict(d);
	mp_arg.pool_name = name;
	mp_arg.d = d;
	rte_mempool_walk(mempool_info_cb, &mp_arg);

	return 0;
}

RTE_INIT(cnxk_mempool_init_telemetry)
{
	rte_telemetry_register_cmd(
		"/cnxk/mempool/info", mempool_tel_handle_info,
		"Returns mempool info. Parameters: pool_name");
}
