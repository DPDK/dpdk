/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_pdcp.h>

#include "pdcp_crypto.h"

int
pdcp_crypto_sess_create(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf)
{
	RTE_SET_USED(entity);
	RTE_SET_USED(conf);
	return 0;
}

void
pdcp_crypto_sess_destroy(struct rte_pdcp_entity *entity)
{
	RTE_SET_USED(entity);
}
