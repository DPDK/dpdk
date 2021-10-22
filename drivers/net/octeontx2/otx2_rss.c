/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"

int
otx2_nix_rss_tbl_init(struct otx2_eth_dev *dev,
		      uint8_t group, uint16_t *ind_tbl)
{
	struct otx2_rss_info *rss = &dev->rss_info;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *req;
	int rc, idx;

	for (idx = 0; idx < rss->rss_size; idx++) {
		req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		if (!req) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0)
				return rc;

			req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
			if (!req)
				return -ENOMEM;
		}
		req->rss.rq = ind_tbl[idx];
		/* Fill AQ info */
		req->qidx = (group * rss->rss_size) + idx;
		req->ctype = NIX_AQ_CTYPE_RSS;
		req->op = NIX_AQ_INSTOP_INIT;

		if (!dev->lock_rx_ctx)
			continue;

		req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		if (!req) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0)
				return rc;

			req = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
			if (!req)
				return -ENOMEM;
		}
		req->rss.rq = ind_tbl[idx];
		/* Fill AQ info */
		req->qidx = (group * rss->rss_size) + idx;
		req->ctype = NIX_AQ_CTYPE_RSS;
		req->op = NIX_AQ_INSTOP_LOCK;
	}

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);
	if (rc < 0)
		return rc;

	return 0;
}

int
otx2_nix_dev_reta_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_rss_info *rss = &dev->rss_info;
	int rc, i, j;
	int idx = 0;

	rc = -EINVAL;
	if (reta_size != dev->rss_info.rss_size) {
		otx2_err("Size of hash lookup table configured "
		"(%d) doesn't match the number hardware can supported "
		"(%d)", reta_size, dev->rss_info.rss_size);
		goto fail;
	}

	/* Copy RETA table */
	for (i = 0; i < (dev->rss_info.rss_size / RTE_ETH_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++) {
			if ((reta_conf[i].mask >> j) & 0x01)
				rss->ind_tbl[idx] = reta_conf[i].reta[j];
			idx++;
		}
	}

	return otx2_nix_rss_tbl_init(dev, 0, dev->rss_info.ind_tbl);

fail:
	return rc;
}

int
otx2_nix_dev_reta_query(struct rte_eth_dev *eth_dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_rss_info *rss = &dev->rss_info;
	int rc, i, j;

	rc = -EINVAL;

	if (reta_size != dev->rss_info.rss_size) {
		otx2_err("Size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, dev->rss_info.rss_size);
		goto fail;
	}

	/* Copy RETA table */
	for (i = 0; i < (dev->rss_info.rss_size / RTE_ETH_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++)
			if ((reta_conf[i].mask >> j) & 0x01)
				reta_conf[i].reta[j] = rss->ind_tbl[j];
	}

	return 0;

fail:
	return rc;
}

void
otx2_nix_rss_set_key(struct otx2_eth_dev *dev, uint8_t *key,
		     uint32_t key_len)
{
	const uint8_t default_key[NIX_HASH_KEY_SIZE] = {
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD
	};
	struct otx2_rss_info *rss = &dev->rss_info;
	uint64_t *keyptr;
	uint64_t val;
	uint32_t idx;

	if (key == NULL || key == 0) {
		keyptr = (uint64_t *)(uintptr_t)default_key;
		key_len = NIX_HASH_KEY_SIZE;
		memset(rss->key, 0, key_len);
	} else {
		memcpy(rss->key, key, key_len);
		keyptr = (uint64_t *)rss->key;
	}

	for (idx = 0; idx < (key_len >> 3); idx++) {
		val = rte_cpu_to_be_64(*keyptr);
		otx2_write64(val, dev->base + NIX_LF_RX_SECRETX(idx));
		keyptr++;
	}
}

static void
rss_get_key(struct otx2_eth_dev *dev, uint8_t *key)
{
	uint64_t *keyptr = (uint64_t *)key;
	uint64_t val;
	int idx;

	for (idx = 0; idx < (NIX_HASH_KEY_SIZE >> 3); idx++) {
		val = otx2_read64(dev->base + NIX_LF_RX_SECRETX(idx));
		*keyptr = rte_be_to_cpu_64(val);
		keyptr++;
	}
}

#define RSS_IPV4_ENABLE ( \
			  RTE_ETH_RSS_IPV4 | \
			  RTE_ETH_RSS_FRAG_IPV4 | \
			  RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
			  RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
			  RTE_ETH_RSS_NONFRAG_IPV4_SCTP)

#define RSS_IPV6_ENABLE ( \
			  RTE_ETH_RSS_IPV6 | \
			  RTE_ETH_RSS_FRAG_IPV6 | \
			  RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
			  RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
			  RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

#define RSS_IPV6_EX_ENABLE ( \
			     RTE_ETH_RSS_IPV6_EX | \
			     RTE_ETH_RSS_IPV6_TCP_EX | \
			     RTE_ETH_RSS_IPV6_UDP_EX)

#define RSS_MAX_LEVELS   3

#define RSS_IPV4_INDEX   0
#define RSS_IPV6_INDEX   1
#define RSS_TCP_INDEX    2
#define RSS_UDP_INDEX    3
#define RSS_SCTP_INDEX   4
#define RSS_DMAC_INDEX   5

uint32_t
otx2_rss_ethdev_to_nix(struct otx2_eth_dev *dev, uint64_t ethdev_rss,
		       uint8_t rss_level)
{
	uint32_t flow_key_type[RSS_MAX_LEVELS][6] = {
		{
			FLOW_KEY_TYPE_IPV4, FLOW_KEY_TYPE_IPV6,
			FLOW_KEY_TYPE_TCP, FLOW_KEY_TYPE_UDP,
			FLOW_KEY_TYPE_SCTP, FLOW_KEY_TYPE_ETH_DMAC
		},
		{
			FLOW_KEY_TYPE_INNR_IPV4, FLOW_KEY_TYPE_INNR_IPV6,
			FLOW_KEY_TYPE_INNR_TCP, FLOW_KEY_TYPE_INNR_UDP,
			FLOW_KEY_TYPE_INNR_SCTP, FLOW_KEY_TYPE_INNR_ETH_DMAC
		},
		{
			FLOW_KEY_TYPE_IPV4 | FLOW_KEY_TYPE_INNR_IPV4,
			FLOW_KEY_TYPE_IPV6 | FLOW_KEY_TYPE_INNR_IPV6,
			FLOW_KEY_TYPE_TCP | FLOW_KEY_TYPE_INNR_TCP,
			FLOW_KEY_TYPE_UDP | FLOW_KEY_TYPE_INNR_UDP,
			FLOW_KEY_TYPE_SCTP | FLOW_KEY_TYPE_INNR_SCTP,
			FLOW_KEY_TYPE_ETH_DMAC | FLOW_KEY_TYPE_INNR_ETH_DMAC
		}
	};
	uint32_t flowkey_cfg = 0;

	dev->rss_info.nix_rss = ethdev_rss;

	if (ethdev_rss & RTE_ETH_RSS_L2_PAYLOAD &&
	    dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_CH_LEN_90B) {
		flowkey_cfg |= FLOW_KEY_TYPE_CH_LEN_90B;
	}

	if (ethdev_rss & RTE_ETH_RSS_C_VLAN)
		flowkey_cfg |= FLOW_KEY_TYPE_VLAN;

	if (ethdev_rss & RTE_ETH_RSS_L3_SRC_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L3_SRC;

	if (ethdev_rss & RTE_ETH_RSS_L3_DST_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L3_DST;

	if (ethdev_rss & RTE_ETH_RSS_L4_SRC_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L4_SRC;

	if (ethdev_rss & RTE_ETH_RSS_L4_DST_ONLY)
		flowkey_cfg |= FLOW_KEY_TYPE_L4_DST;

	if (ethdev_rss & RSS_IPV4_ENABLE)
		flowkey_cfg |= flow_key_type[rss_level][RSS_IPV4_INDEX];

	if (ethdev_rss & RSS_IPV6_ENABLE)
		flowkey_cfg |= flow_key_type[rss_level][RSS_IPV6_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_TCP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_TCP_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_UDP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_UDP_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_SCTP)
		flowkey_cfg |= flow_key_type[rss_level][RSS_SCTP_INDEX];

	if (ethdev_rss & RTE_ETH_RSS_L2_PAYLOAD)
		flowkey_cfg |= flow_key_type[rss_level][RSS_DMAC_INDEX];

	if (ethdev_rss & RSS_IPV6_EX_ENABLE)
		flowkey_cfg |= FLOW_KEY_TYPE_IPV6_EXT;

	if (ethdev_rss & RTE_ETH_RSS_PORT)
		flowkey_cfg |= FLOW_KEY_TYPE_PORT;

	if (ethdev_rss & RTE_ETH_RSS_NVGRE)
		flowkey_cfg |= FLOW_KEY_TYPE_NVGRE;

	if (ethdev_rss & RTE_ETH_RSS_VXLAN)
		flowkey_cfg |= FLOW_KEY_TYPE_VXLAN;

	if (ethdev_rss & RTE_ETH_RSS_GENEVE)
		flowkey_cfg |= FLOW_KEY_TYPE_GENEVE;

	if (ethdev_rss & RTE_ETH_RSS_GTPU)
		flowkey_cfg |= FLOW_KEY_TYPE_GTPU;

	return flowkey_cfg;
}

int
otx2_rss_set_hf(struct otx2_eth_dev *dev, uint32_t flowkey_cfg,
		uint8_t *alg_idx, uint8_t group, int mcam_index)
{
	struct nix_rss_flowkey_cfg_rsp *rss_rsp;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_rss_flowkey_cfg *cfg;
	int rc;

	rc = -EINVAL;

	dev->rss_info.flowkey_cfg = flowkey_cfg;

	cfg = otx2_mbox_alloc_msg_nix_rss_flowkey_cfg(mbox);

	cfg->flowkey_cfg = flowkey_cfg;
	cfg->mcam_index = mcam_index; /* -1 indicates default group */
	cfg->group = group; /* 0 is default group */

	rc = otx2_mbox_process_msg(mbox, (void *)&rss_rsp);
	if (rc)
		return rc;

	if (alg_idx)
		*alg_idx = rss_rsp->alg_idx;

	return rc;
}

int
otx2_nix_rss_hash_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint8_t rss_hash_level;
	uint32_t flowkey_cfg;
	uint8_t alg_idx;
	int rc;

	rc = -EINVAL;

	if (rss_conf->rss_key && rss_conf->rss_key_len != NIX_HASH_KEY_SIZE) {
		otx2_err("Hash key size mismatch %d vs %d",
			 rss_conf->rss_key_len, NIX_HASH_KEY_SIZE);
		goto fail;
	}

	if (rss_conf->rss_key)
		otx2_nix_rss_set_key(dev, rss_conf->rss_key,
				     (uint32_t)rss_conf->rss_key_len);

	rss_hash_level = RTE_ETH_RSS_LEVEL(rss_conf->rss_hf);
	if (rss_hash_level)
		rss_hash_level -= 1;
	flowkey_cfg =
		otx2_rss_ethdev_to_nix(dev, rss_conf->rss_hf, rss_hash_level);

	rc = otx2_rss_set_hf(dev, flowkey_cfg, &alg_idx,
			     NIX_DEFAULT_RSS_CTX_GROUP,
			     NIX_DEFAULT_RSS_MCAM_IDX);
	if (rc) {
		otx2_err("Failed to set RSS hash function rc=%d", rc);
		return rc;
	}

	dev->rss_info.alg_idx = alg_idx;

fail:
	return rc;
}

int
otx2_nix_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	if (rss_conf->rss_key)
		rss_get_key(dev, rss_conf->rss_key);

	rss_conf->rss_key_len = NIX_HASH_KEY_SIZE;
	rss_conf->rss_hf = dev->rss_info.nix_rss;

	return 0;
}

int
otx2_nix_rss_config(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint32_t idx, qcnt = eth_dev->data->nb_rx_queues;
	uint8_t rss_hash_level;
	uint32_t flowkey_cfg;
	uint64_t rss_hf;
	uint8_t alg_idx;
	int rc;

	/* Skip further configuration if selected mode is not RSS */
	if (eth_dev->data->dev_conf.rxmode.mq_mode != RTE_ETH_MQ_RX_RSS || !qcnt)
		return 0;

	/* Update default RSS key and cfg */
	otx2_nix_rss_set_key(dev, NULL, 0);

	/* Update default RSS RETA */
	for (idx = 0; idx < dev->rss_info.rss_size; idx++)
		dev->rss_info.ind_tbl[idx] = idx % qcnt;

	/* Init RSS table context */
	rc = otx2_nix_rss_tbl_init(dev, 0, dev->rss_info.ind_tbl);
	if (rc) {
		otx2_err("Failed to init RSS table rc=%d", rc);
		return rc;
	}

	rss_hf = eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	rss_hash_level = RTE_ETH_RSS_LEVEL(rss_hf);
	if (rss_hash_level)
		rss_hash_level -= 1;
	flowkey_cfg = otx2_rss_ethdev_to_nix(dev, rss_hf, rss_hash_level);

	rc = otx2_rss_set_hf(dev, flowkey_cfg, &alg_idx,
			     NIX_DEFAULT_RSS_CTX_GROUP,
			     NIX_DEFAULT_RSS_MCAM_IDX);
	if (rc) {
		otx2_err("Failed to set RSS hash function rc=%d", rc);
		return rc;
	}

	dev->rss_info.alg_idx = alg_idx;

	return 0;
}
