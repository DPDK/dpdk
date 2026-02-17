/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 CESNET, z.s.p.o.
 */

#include <eal_export.h>
#include <rte_ethdev.h>

#include "nfb_mdio.h"

RTE_EXPORT_INTERNAL_SYMBOL(nfb_mdio_cl45_pma_get_speed_capa)
void
nfb_mdio_cl45_pma_get_speed_capa(struct mdio_if_info *info, uint32_t *capa)
{
	int i;
	int reg;

	const int speed_ability[NFB_MDIO_WIDTH] = {
		RTE_ETH_LINK_SPEED_10G,
		0,
		0,
		RTE_ETH_LINK_SPEED_50G,
		RTE_ETH_LINK_SPEED_1G,
		RTE_ETH_LINK_SPEED_100M,
		RTE_ETH_LINK_SPEED_10M,
		0,
		RTE_ETH_LINK_SPEED_40G,
		RTE_ETH_LINK_SPEED_100G,
		0,
		RTE_ETH_LINK_SPEED_25G,
		RTE_ETH_LINK_SPEED_200G,
		RTE_ETH_LINK_SPEED_2_5G,
		RTE_ETH_LINK_SPEED_5G,
		RTE_ETH_LINK_SPEED_400G,
	};

	reg = nfb_mdio_if_read_pma(info, NFB_MDIO_PMA_SPEED_ABILITY);
	if (reg < 0)
		return;

	for (i = 0; i < NFB_MDIO_WIDTH; i++) {
		if (reg & NFB_MDIO_BIT(i))
			*capa |= speed_ability[i];
	}
}
