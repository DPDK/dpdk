/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 CESNET, z.s.p.o.
 */

#ifndef _NFB_MDIO_H_
#define _NFB_MDIO_H_

#include <stdint.h>
#include <rte_bitops.h>
#include <netcope/mdio_if_info.h>


#define NFB_MDIO_WIDTH (UINT16_WIDTH)
#define NFB_MDIO_BIT(nr) (UINT16_C(1) << (nr))

#define NFB_MDIO_DEV_PMA 1

#define NFB_MDIO_PMA_CTRL 0
#define NFB_MDIO_PMA_CTRL_RESET NFB_MDIO_BIT(15)

#define NFB_MDIO_PMA_SPEED_ABILITY 4

#define NFB_MDIO_PMA_RSFEC_CR 200
#define NFB_MDIO_PMA_RSFEC_CR_ENABLE NFB_MDIO_BIT(2)

static inline int nfb_mdio_if_read_pma(struct mdio_if_info *if_info, uint16_t addr)
{
	return if_info->mdio_read(if_info->dev, if_info->prtad, NFB_MDIO_DEV_PMA, addr);
}

static inline int nfb_mdio_if_write_pma(struct mdio_if_info *if_info, uint16_t addr, uint16_t val)
{
	return if_info->mdio_write(if_info->dev, if_info->prtad, NFB_MDIO_DEV_PMA, addr, val);
}

__rte_internal
void nfb_mdio_cl45_pma_get_speed_capa(struct mdio_if_info *if_info, uint32_t *capa);

#endif /* _NFB_MDIO_H_ */
