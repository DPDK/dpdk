/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_ethdev.h"

static int
nix_mc_addr_list_free(struct otx2_eth_dev *dev, uint32_t entry_count)
{
	struct npc_mcam_free_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct mcast_entry *entry;
	int rc = 0;

	if (entry_count == 0)
		goto exit;

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next) {
		req = otx2_mbox_alloc_msg_npc_mcam_free_entry(mbox);
		req->entry = entry->mcam_index;

		rc = otx2_mbox_process_msg(mbox, NULL);
		if (rc < 0)
			goto exit;

		TAILQ_REMOVE(&dev->mc_fltr_tbl, entry, next);
		rte_free(entry);
		entry_count--;

		if (entry_count == 0)
			break;
	}

	if (entry == NULL)
		dev->mc_tbl_set = false;

exit:
	return rc;
}

static int
nix_hw_update_mc_addr_list(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_npc_flow_info *npc = &dev->npc_flow;
	volatile uint8_t *key_data, *key_mask;
	struct npc_mcam_write_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct npc_xtract_info *x_info;
	uint64_t mcam_data, mcam_mask;
	struct mcast_entry *entry;
	otx2_dxcfg_t *ld_cfg;
	uint8_t *mac_addr;
	uint64_t action;
	int idx, rc = 0;

	ld_cfg = &npc->prx_dxcfg;
	/* Get ETH layer profile info for populating mcam entries */
	x_info = &(*ld_cfg)[NPC_MCAM_RX][NPC_LID_LA][NPC_LT_LA_ETHER].xtract[0];

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next) {
		req = otx2_mbox_alloc_msg_npc_mcam_write_entry(mbox);
		if (req == NULL) {
			/* The mbox memory buffer can be full.
			 * Flush it and retry
			 */
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0)
				goto exit;

			req = otx2_mbox_alloc_msg_npc_mcam_write_entry(mbox);
			if (req == NULL) {
				rc = -ENOMEM;
				goto exit;
			}
		}
		req->entry = entry->mcam_index;
		req->intf = NPC_MCAM_RX;
		req->enable_entry = 1;

		/* Channel base extracted to KW0[11:0] */
		req->entry_data.kw[0] = dev->rx_chan_base;
		req->entry_data.kw_mask[0] = RTE_LEN2MASK(12, uint64_t);

		/* Update mcam address */
		key_data = (volatile uint8_t *)req->entry_data.kw;
		key_mask = (volatile uint8_t *)req->entry_data.kw_mask;

		mcam_data = 0ull;
		mcam_mask = RTE_LEN2MASK(48, uint64_t);
		mac_addr = &entry->mcast_mac.addr_bytes[0];
		for (idx = RTE_ETHER_ADDR_LEN - 1; idx >= 0; idx--)
			mcam_data |= ((uint64_t)*mac_addr++) << (8 * idx);

		otx2_mbox_memcpy(key_data + x_info->key_off,
				 &mcam_data, x_info->len);
		otx2_mbox_memcpy(key_mask + x_info->key_off,
				 &mcam_mask, x_info->len);

		action = NIX_RX_ACTIONOP_UCAST;

		if (eth_dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
			action = NIX_RX_ACTIONOP_RSS;
			action |= (uint64_t)(dev->rss_info.alg_idx) << 56;
		}

		action |= ((uint64_t)otx2_pfvf_func(dev->pf, dev->vf)) << 4;
		req->entry_data.action = action;
	}

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);

exit:
	return rc;
}

int
otx2_nix_mc_addr_list_install(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	struct otx2_mbox *mbox = dev->mbox;
	uint32_t entry_count = 0, idx  = 0;
	struct mcast_entry *entry;
	int rc = 0;

	if (!dev->mc_tbl_set)
		return 0;

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next)
		entry_count++;

	req = otx2_mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	req->priority = NPC_MCAM_ANY_PRIO;
	req->count = entry_count;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc || rsp->count  < entry_count) {
		otx2_err("Failed to allocate required mcam entries");
		goto exit;
	}

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next)
		entry->mcam_index = rsp->entry_list[idx];

	rc = nix_hw_update_mc_addr_list(eth_dev);

exit:
	return rc;
}

int
otx2_nix_mc_addr_list_uninstall(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct npc_mcam_free_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct mcast_entry *entry;
	int rc = 0;

	if (!dev->mc_tbl_set)
		return 0;

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next) {
		req = otx2_mbox_alloc_msg_npc_mcam_free_entry(mbox);
		if (req == NULL) {
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0)
				goto exit;

			req = otx2_mbox_alloc_msg_npc_mcam_free_entry(mbox);
			if (req == NULL) {
				rc = -ENOMEM;
				goto exit;
			}
		}
		req->entry = entry->mcam_index;
	}

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);

exit:
	return rc;
}

static int
nix_setup_mc_addr_list(struct otx2_eth_dev *dev,
		       struct rte_ether_addr *mc_addr_set)
{
	struct npc_mcam_ena_dis_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct mcast_entry *entry;
	uint32_t idx = 0;
	int rc = 0;

	/* Populate PMD's mcast list with given mcast mac addresses and
	 * disable all mcam entries pertaining to the mcast list.
	 */
	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next) {
		rte_memcpy(&entry->mcast_mac, &mc_addr_set[idx++],
			   RTE_ETHER_ADDR_LEN);

		req = otx2_mbox_alloc_msg_npc_mcam_dis_entry(mbox);
		if (req == NULL) {
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0)
				goto exit;

			req = otx2_mbox_alloc_msg_npc_mcam_dis_entry(mbox);
			if (req == NULL) {
				rc = -ENOMEM;
				goto exit;
			}
		}
		req->entry = entry->mcam_index;
	}

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_wait_for_rsp(mbox, 0);

exit:
	return rc;
}

int
otx2_nix_set_mc_addr_list(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *mc_addr_set,
			  uint32_t nb_mc_addr)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	struct otx2_mbox *mbox = dev->mbox;
	uint32_t idx, priv_count = 0;
	struct mcast_entry *entry;
	int rc = 0;

	if (otx2_dev_is_vf(dev))
		return -ENOTSUP;

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next)
		priv_count++;

	if (nb_mc_addr == 0 || mc_addr_set == NULL) {
		/* Free existing list if new list is null */
		nb_mc_addr = priv_count;
		goto exit;
	}

	for (idx = 0; idx < nb_mc_addr; idx++) {
		if (!rte_is_multicast_ether_addr(&mc_addr_set[idx]))
			return -EINVAL;
	}

	/* New list is bigger than the existing list,
	 * allocate mcam entries for the extra entries.
	 */
	if (nb_mc_addr > priv_count) {
		req = otx2_mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
		req->priority = NPC_MCAM_ANY_PRIO;
		req->count = nb_mc_addr - priv_count;

		rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (rc || (rsp->count + priv_count < nb_mc_addr)) {
			otx2_err("Failed to allocate required entries");
			nb_mc_addr = priv_count;
			goto exit;
		}

		/* Append new mcam entries to the existing mc list */
		for (idx = 0; idx < rsp->count; idx++) {
			entry = rte_zmalloc("otx2_nix_mc_entry",
					    sizeof(struct mcast_entry), 0);
			if (!entry) {
				otx2_err("Failed to allocate memory");
				nb_mc_addr = priv_count;
				rc = -ENOMEM;
				goto exit;
			}
			entry->mcam_index = rsp->entry_list[idx];
			TAILQ_INSERT_HEAD(&dev->mc_fltr_tbl, entry, next);
		}
	} else {
		/* Free the extra mcam entries if the new list is smaller
		 * than exiting list.
		 */
		nix_mc_addr_list_free(dev, priv_count - nb_mc_addr);
	}


	/* Now mc_fltr_tbl has the required number of mcam entries,
	 * Traverse through it and add new multicast filter table entries.
	 */
	rc = nix_setup_mc_addr_list(dev, mc_addr_set);
	if (rc < 0)
		goto exit;

	rc = nix_hw_update_mc_addr_list(eth_dev);
	if (rc < 0)
		goto exit;

	dev->mc_tbl_set = true;

	return 0;

exit:
	nix_mc_addr_list_free(dev, nb_mc_addr);
	return rc;
}

void
otx2_nix_mc_filter_init(struct otx2_eth_dev *dev)
{
	if (otx2_dev_is_vf(dev))
		return;

	TAILQ_INIT(&dev->mc_fltr_tbl);
}

void
otx2_nix_mc_filter_fini(struct otx2_eth_dev *dev)
{
	struct mcast_entry *entry;
	uint32_t count = 0;

	if (otx2_dev_is_vf(dev))
		return;

	TAILQ_FOREACH(entry, &dev->mc_fltr_tbl, next)
		count++;

	nix_mc_addr_list_free(dev, count);
}
