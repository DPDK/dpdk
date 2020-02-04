/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_security.h>

#include "otx2_ethdev.h"
#include "otx2_ethdev_sec.h"
#include "otx2_ipsec_fp.h"

#define ETH_SEC_MAX_PKT_LEN	1450

struct eth_sec_tag_const {
	RTE_STD_C11
	union {
		struct {
			uint32_t rsvd_11_0  : 12;
			uint32_t port       : 8;
			uint32_t event_type : 4;
			uint32_t rsvd_31_24 : 8;
		};
		uint32_t u32;
	};
};

static inline void
in_sa_mz_name_get(char *name, int size, uint16_t port)
{
	snprintf(name, size, "otx2_ipsec_in_sadb_%u", port);
}

int
otx2_eth_sec_ctx_create(struct rte_eth_dev *eth_dev)
{
	struct rte_security_ctx *ctx;

	ctx = rte_malloc("otx2_eth_sec_ctx",
			 sizeof(struct rte_security_ctx), 0);
	if (ctx == NULL)
		return -ENOMEM;

	/* Populate ctx */

	ctx->device = eth_dev;
	ctx->sess_cnt = 0;

	eth_dev->security_ctx = ctx;

	return 0;
}

void
otx2_eth_sec_ctx_destroy(struct rte_eth_dev *eth_dev)
{
	rte_free(eth_dev->security_ctx);
}

static int
eth_sec_ipsec_cfg(struct rte_eth_dev *eth_dev, uint8_t tt)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t port = eth_dev->data->port_id;
	struct nix_inline_ipsec_lf_cfg *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct eth_sec_tag_const tag_const;
	char name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	mz = rte_memzone_lookup(name);
	if (mz == NULL)
		return -EINVAL;

	req = otx2_mbox_alloc_msg_nix_inline_ipsec_lf_cfg(mbox);
	req->enable = 1;
	req->sa_base_addr = mz->iova;

	req->ipsec_cfg0.tt = tt;

	tag_const.u32 = 0;
	tag_const.event_type = RTE_EVENT_TYPE_ETHDEV;
	tag_const.port = port;
	req->ipsec_cfg0.tag_const = tag_const.u32;

	req->ipsec_cfg0.sa_pow2_size =
			rte_log2_u32(sizeof(struct otx2_ipsec_fp_in_sa));
	req->ipsec_cfg0.lenm1_max = ETH_SEC_MAX_PKT_LEN - 1;

	req->ipsec_cfg1.sa_idx_w = rte_log2_u32(dev->ipsec_in_max_spi);
	req->ipsec_cfg1.sa_idx_max = dev->ipsec_in_max_spi - 1;

	return otx2_mbox_process(mbox);
}

int
otx2_eth_sec_init(struct rte_eth_dev *eth_dev)
{
	const size_t sa_width = sizeof(struct otx2_ipsec_fp_in_sa);
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t port = eth_dev->data->port_id;
	char name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int mz_sz, ret;
	uint16_t nb_sa;

	RTE_BUILD_BUG_ON(sa_width < 32 || sa_width > 512 ||
			 !RTE_IS_POWER_OF_2(sa_width));

	if (!(dev->tx_offloads & DEV_TX_OFFLOAD_SECURITY) &&
	    !(dev->rx_offloads & DEV_RX_OFFLOAD_SECURITY))
		return 0;

	nb_sa = dev->ipsec_in_max_spi;
	mz_sz = nb_sa * sa_width;
	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	mz = rte_memzone_reserve_aligned(name, mz_sz, rte_socket_id(),
					 RTE_MEMZONE_IOVA_CONTIG, OTX2_ALIGN);

	if (mz == NULL) {
		otx2_err("Could not allocate inbound SA DB");
		return -ENOMEM;
	}

	memset(mz->addr, 0, mz_sz);

	ret = eth_sec_ipsec_cfg(eth_dev, SSO_TT_ORDERED);
	if (ret < 0) {
		otx2_err("Could not configure inline IPsec");
		goto sec_fini;
	}

	return 0;

sec_fini:
	otx2_err("Could not configure device for security");
	otx2_eth_sec_fini(eth_dev);
	return ret;
}

void
otx2_eth_sec_fini(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint16_t port = eth_dev->data->port_id;
	char name[RTE_MEMZONE_NAMESIZE];

	if (!(dev->tx_offloads & DEV_TX_OFFLOAD_SECURITY) &&
	    !(dev->rx_offloads & DEV_RX_OFFLOAD_SECURITY))
		return;

	in_sa_mz_name_get(name, RTE_MEMZONE_NAMESIZE, port);
	rte_memzone_free(rte_memzone_lookup(name));
}
