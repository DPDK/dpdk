/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_pdcp.h>

#include "pdcp_cnt.h"
#include "pdcp_entity.h"

int
pdcp_cnt_ring_create(struct rte_pdcp_entity *en, const struct rte_pdcp_entity_conf *conf)
{
	struct entity_priv_dl_part *en_priv_dl;
	uint32_t window_sz;

	if (en == NULL || conf == NULL)
		return -EINVAL;

	if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
		return 0;

	en_priv_dl = entity_dl_part_get(en);
	window_sz = pdcp_window_size_get(conf->pdcp_xfrm.sn_size);

	RTE_SET_USED(window_sz);
	RTE_SET_USED(en_priv_dl);

	return 0;
}
