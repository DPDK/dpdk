/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#ifndef _MACB_ETHDEV_H_
#define _MACB_ETHDEV_H_

#include <rte_interrupts.h>
#include <dlfcn.h>
#include "base/macb_common.h"
#include "macb_log.h"

#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_MIN_MTU	68		/* Min IPv4 MTU per RFC791	*/

#define CLK_STR_LEN 64
#define PHY_MODE_LEN 64
#define PHY_ADDR_LEN 64
#define DEV_TYPE_LEN 64
#define SPEED_INFO_LEN 64
#define MAX_PHY_AD_NUM 32
#define PHY_ID_OFFSET 16

#define GEM_MTU_MIN_SIZE	ETH_MIN_MTU

#ifndef min
#define min(x, y) ({ \
		typeof(x) _x = (x); \
		typeof(y) _y = (y); \
		(_x < _y) ? _x : _y; \
		})
#endif

/*
 * Custom phy driver need to be stated here.
 */
extern struct phy_driver genphy_driver;

/*internal macb 10G PHY*/
extern struct phy_driver macb_usxgmii_pcs_driver;
/*internal macb gbe PHY*/
extern struct phy_driver macb_gbe_pcs_driver;

#define VLAN_TAG_SIZE    4
#define RTE_ETHER_CRC_LEN   4 /**< Length of Ethernet CRC. */
#define RTE_ETHER_TYPE_LEN 2
#define RTE_ETHER_ADDR_LEN 6
#define RTE_ETHER_HDR_LEN   \
	(RTE_ETHER_ADDR_LEN * 2 + \
	 RTE_ETHER_TYPE_LEN) /**< Length of Ethernet header. */
#define MACB_ETH_OVERHEAD (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + \
				VLAN_TAG_SIZE)

#define MACB_RX_INT_FLAGS	(MACB_BIT(RCOMP) | MACB_BIT(ISR_ROVR))
#define MACB_TX_ERR_FLAGS	(MACB_BIT(ISR_TUND)			\
					| MACB_BIT(ISR_RLE)		\
					| MACB_BIT(TXERR))
#define MACB_TX_INT_FLAGS	(MACB_TX_ERR_FLAGS | MACB_BIT(TCOMP)	\
					| MACB_BIT(TXUBR))

struct macb_priv {
	struct macb *bp;
	uint32_t port_id;
	uint64_t pclk_hz;
	phys_addr_t physical_addr;
	uint32_t dev_type;
	bool stopped;
	netdev_features_t hw_features;
	netdev_features_t phy_interface;
	struct rte_eth_stats prev_stats;
	struct rte_intr_handle *intr_handle;
	char name[RTE_ETH_NAME_MAX_LEN];
};

#endif /* _MACB_ETHDEV_H_ */
