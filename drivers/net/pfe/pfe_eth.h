/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _PFE_ETH_H_
#define _PFE_ETH_H_

#include <compat.h>
#include <rte_ethdev.h>
#include <ethdev_vdev.h>

#define ETH_ALEN 6
#define GEMAC_NO_PHY            BIT(0)

#define PFE_SOC_ID_FILE	"/sys/devices/soc0/soc_id"
extern unsigned int pfe_svr;
#define SVR_LS1012A_REV2	0x87040020
#define SVR_LS1012A_REV1	0x87040010

#define PFE_ETH_OVERHEAD        (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN)
#define MAX_MTU_ON_REV1         1878
struct ls1012a_eth_platform_data {
	/* device specific information */
	u32 device_flags;
	char name[16];

	/* board specific information */
	u32 mii_config;
	u32 phy_flags;
	u32 gem_id;
	u32 bus_id;
	u32 phy_id;
	u32 mdio_muxval;
	u8 mac_addr[ETH_ALEN];
};

struct ls1012a_mdio_platform_data {
	int enabled;
	int irq[32];
	u32 phy_mask;
	int mdc_div;
};

struct ls1012a_pfe_platform_data {
	struct ls1012a_eth_platform_data ls1012a_eth_pdata[3];
	struct ls1012a_mdio_platform_data ls1012a_mdio_pdata[3];
};

#define EMAC_TXQ_CNT	16
#define EMAC_TXQ_DEPTH	(HIF_TX_DESC_NT)

#define JUMBO_FRAME_SIZE	10258
#define EMAC_RXQ_CNT	1
#define EMAC_RXQ_DEPTH	HIF_RX_DESC_NT

struct  pfe_eth_priv_s {
	struct pfe		*pfe;
	struct hif_client_s	client;
	int			low_tmu_q;
	int			high_tmu_q;
	struct rte_eth_dev	*ndev;
	struct rte_eth_stats	stats;
	int			id;
	int			promisc;
	int			link_fd;

	spinlock_t		lock; /* protect member variables */
	void			*EMAC_baseaddr;
	/* This points to the EMAC base from where we access PHY */
	void			*PHY_baseaddr;
	void			*GPI_baseaddr;

	struct ls1012a_eth_platform_data *einfo;
};

#endif /* _PFE_ETH_H_ */
