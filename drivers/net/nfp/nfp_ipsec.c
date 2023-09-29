/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine Systems, Inc.
 * All rights reserved.
 */

#include "nfp_ipsec.h"

#include <rte_malloc.h>
#include <rte_security_driver.h>

#include <ethdev_driver.h>
#include <ethdev_pci.h>

#include "nfp_common.h"
#include "nfp_ctrl.h"
#include "nfp_logs.h"
#include "nfp_rxtx.h"

static const struct rte_security_ops nfp_security_ops;

static int
nfp_ipsec_ctx_create(struct rte_eth_dev *dev,
		struct nfp_net_ipsec_data *data)
{
	struct rte_security_ctx *ctx;
	static const struct rte_mbuf_dynfield pkt_md_dynfield = {
		.name = "nfp_ipsec_crypto_pkt_metadata",
		.size = sizeof(struct nfp_tx_ipsec_desc_msg),
		.align = __alignof__(struct nfp_tx_ipsec_desc_msg),
	};

	ctx = rte_zmalloc("security_ctx",
			sizeof(struct rte_security_ctx), 0);
	if (ctx == NULL) {
		PMD_INIT_LOG(ERR, "Failed to malloc security_ctx");
		return -ENOMEM;
	}

	ctx->device = dev;
	ctx->ops = &nfp_security_ops;
	ctx->sess_cnt = 0;
	dev->security_ctx = ctx;

	data->pkt_dynfield_offset = rte_mbuf_dynfield_register(&pkt_md_dynfield);
	if (data->pkt_dynfield_offset < 0) {
		PMD_INIT_LOG(ERR, "Failed to register mbuf esn_dynfield");
		return -ENOMEM;
	}

	return 0;
}

int
nfp_ipsec_init(struct rte_eth_dev *dev)
{
	int ret;
	uint32_t cap_extend;
	struct nfp_net_hw *hw;
	struct nfp_net_ipsec_data *data;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	cap_extend = nn_cfg_readl(hw, NFP_NET_CFG_CAP_WORD1);
	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) == 0) {
		PMD_INIT_LOG(INFO, "Unsupported IPsec extend capability");
		return 0;
	}

	data = rte_zmalloc("ipsec_data", sizeof(struct nfp_net_ipsec_data), 0);
	if (data == NULL) {
		PMD_INIT_LOG(ERR, "Failed to malloc ipsec_data");
		return -ENOMEM;
	}

	data->pkt_dynfield_offset = -1;
	data->sa_free_cnt = NFP_NET_IPSEC_MAX_SA_CNT;
	hw->ipsec_data = data;

	ret = nfp_ipsec_ctx_create(dev, data);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to create IPsec ctx");
		goto ipsec_cleanup;
	}

	return 0;

ipsec_cleanup:
	nfp_ipsec_uninit(dev);

	return ret;
}

static void
nfp_ipsec_ctx_destroy(struct rte_eth_dev *dev)
{
	if (dev->security_ctx != NULL)
		rte_free(dev->security_ctx);
}

void
nfp_ipsec_uninit(struct rte_eth_dev *dev)
{
	uint16_t i;
	uint32_t cap_extend;
	struct nfp_net_hw *hw;
	struct nfp_ipsec_session *priv_session;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	cap_extend = nn_cfg_readl(hw, NFP_NET_CFG_CAP_WORD1);
	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) == 0) {
		PMD_INIT_LOG(INFO, "Unsupported IPsec extend capability");
		return;
	}

	nfp_ipsec_ctx_destroy(dev);

	if (hw->ipsec_data == NULL) {
		PMD_INIT_LOG(INFO, "IPsec data is NULL!");
		return;
	}

	for (i = 0; i < NFP_NET_IPSEC_MAX_SA_CNT; i++) {
		priv_session = hw->ipsec_data->sa_entries[i];
		if (priv_session != NULL)
			memset(priv_session, 0, sizeof(struct nfp_ipsec_session));
	}

	rte_free(hw->ipsec_data);
}

