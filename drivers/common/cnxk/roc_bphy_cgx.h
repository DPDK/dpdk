/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_BPHY_CGX_H_
#define _ROC_BPHY_CGX_H_

#include <pthread.h>

#include "roc_api.h"

#define MAX_LMACS_PER_CGX 4

struct roc_bphy_cgx {
	uint64_t bar0_pa;
	void *bar0_va;
	uint64_t lmac_bmap;
	unsigned int id;
	/* serialize access to the whole structure */
	pthread_mutex_t lock;
} __plt_cache_aligned;

enum roc_bphy_cgx_eth_link_speed {
	ROC_BPHY_CGX_ETH_LINK_SPEED_NONE,
	ROC_BPHY_CGX_ETH_LINK_SPEED_10M,
	ROC_BPHY_CGX_ETH_LINK_SPEED_100M,
	ROC_BPHY_CGX_ETH_LINK_SPEED_1G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_2HG,
	ROC_BPHY_CGX_ETH_LINK_SPEED_5G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_10G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_20G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_25G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_40G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_50G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_80G,
	ROC_BPHY_CGX_ETH_LINK_SPEED_100G,
	__ROC_BPHY_CGX_ETH_LINK_SPEED_MAX
};

enum roc_bphy_cgx_eth_link_fec {
	ROC_BPHY_CGX_ETH_LINK_FEC_NONE,
	ROC_BPHY_CGX_ETH_LINK_FEC_BASE_R,
	ROC_BPHY_CGX_ETH_LINK_FEC_RS,
	__ROC_BPHY_CGX_ETH_LINK_FEC_MAX
};

enum roc_bphy_cgx_eth_link_mode {
	ROC_BPHY_CGX_ETH_LINK_MODE_SGMII_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_1000_BASEX_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_QSGMII_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_10G_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_10G_C2M_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_10G_KR_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_20G_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_25G_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_25G_C2M_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_25G_2_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_25G_CR_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_25G_KR_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_40G_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_40G_C2M_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_40G_CR4_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_40G_KR4_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_40GAUI_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_50G_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_50G_C2M_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_50G_4_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_50G_CR_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_50G_KR_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_80GAUI_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_100G_C2C_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_100G_C2M_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_100G_CR4_BIT,
	ROC_BPHY_CGX_ETH_LINK_MODE_100G_KR4_BIT,
	__ROC_BPHY_CGX_ETH_LINK_MODE_MAX
};

struct roc_bphy_cgx_link_mode {
	bool full_duplex;
	bool an;
	unsigned int port;
	enum roc_bphy_cgx_eth_link_speed speed;
	enum roc_bphy_cgx_eth_link_mode mode;
};

struct roc_bphy_cgx_link_info {
	bool link_up;
	bool full_duplex;
	enum roc_bphy_cgx_eth_link_speed speed;
	bool an;
	enum roc_bphy_cgx_eth_link_fec fec;
	enum roc_bphy_cgx_eth_link_mode mode;
};

__roc_api int roc_bphy_cgx_dev_init(struct roc_bphy_cgx *roc_cgx);
__roc_api int roc_bphy_cgx_dev_fini(struct roc_bphy_cgx *roc_cgx);

__roc_api int roc_bphy_cgx_start_rxtx(struct roc_bphy_cgx *roc_cgx,
				      unsigned int lmac);
__roc_api int roc_bphy_cgx_stop_rxtx(struct roc_bphy_cgx *roc_cgx,
				     unsigned int lmac);
__roc_api int roc_bphy_cgx_set_link_state(struct roc_bphy_cgx *roc_cgx,
					  unsigned int lmac, bool state);
__roc_api int roc_bphy_cgx_get_linkinfo(struct roc_bphy_cgx *roc_cgx,
					unsigned int lmac,
					struct roc_bphy_cgx_link_info *info);
__roc_api int roc_bphy_cgx_set_link_mode(struct roc_bphy_cgx *roc_cgx,
					 unsigned int lmac,
					 struct roc_bphy_cgx_link_mode *mode);
__roc_api int roc_bphy_cgx_intlbk_enable(struct roc_bphy_cgx *roc_cgx,
					 unsigned int lmac);
__roc_api int roc_bphy_cgx_intlbk_disable(struct roc_bphy_cgx *roc_cgx,
					  unsigned int lmac);
__roc_api int roc_bphy_cgx_ptp_rx_enable(struct roc_bphy_cgx *roc_cgx,
					 unsigned int lmac);
__roc_api int roc_bphy_cgx_ptp_rx_disable(struct roc_bphy_cgx *roc_cgx,
					  unsigned int lmac);
__roc_api int roc_bphy_cgx_fec_set(struct roc_bphy_cgx *roc_cgx,
				   unsigned int lmac,
				   enum roc_bphy_cgx_eth_link_fec fec);
__roc_api int roc_bphy_cgx_fec_supported_get(struct roc_bphy_cgx *roc_cgx,
					     unsigned int lmac,
					     enum roc_bphy_cgx_eth_link_fec *fec);


#endif /* _ROC_BPHY_CGX_H_ */
