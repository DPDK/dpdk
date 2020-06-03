/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_common.h>

#include "otx2_dev.h"
#include "otx2_ethdev.h"

int
otx2_cgx_mac_addr_set(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct cgx_mac_addr_set_or_get *req;
	struct otx2_mbox *mbox = dev->mbox;
	int rc;

	if (otx2_dev_is_vf_or_sdp(dev))
		return -ENOTSUP;

	if (otx2_dev_active_vfs(dev))
		return -ENOTSUP;

	req = otx2_mbox_alloc_msg_cgx_mac_addr_set(mbox);
	otx2_mbox_memcpy(req->mac_addr, addr->addr_bytes, RTE_ETHER_ADDR_LEN);

	rc = otx2_mbox_process(mbox);
	if (rc)
		otx2_err("Failed to set mac address in CGX, rc=%d", rc);

	return 0;
}

int
otx2_cgx_mac_max_entries_get(struct otx2_eth_dev *dev)
{
	struct cgx_max_dmac_entries_get_rsp *rsp;
	struct otx2_mbox *mbox = dev->mbox;
	int rc;

	if (otx2_dev_is_vf_or_sdp(dev))
		return 0;

	otx2_mbox_alloc_msg_cgx_mac_max_entries_get(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	return rsp->max_dmac_filters;
}

int
otx2_nix_mac_addr_add(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr,
		      uint32_t index __rte_unused, uint32_t pool __rte_unused)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct cgx_mac_addr_add_req *req;
	struct cgx_mac_addr_add_rsp *rsp;
	int rc;

	if (otx2_dev_is_vf_or_sdp(dev))
		return -ENOTSUP;

	if (otx2_dev_active_vfs(dev))
		return -ENOTSUP;

	req = otx2_mbox_alloc_msg_cgx_mac_addr_add(mbox);
	otx2_mbox_memcpy(req->mac_addr, addr->addr_bytes, RTE_ETHER_ADDR_LEN);

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		otx2_err("Failed to add mac address, rc=%d", rc);
		goto done;
	}

	/* Enable promiscuous mode at NIX level */
	otx2_nix_promisc_config(eth_dev, 1);
	dev->dmac_filter_enable = true;
	eth_dev->data->promiscuous = 0;

done:
	return rc;
}

void
otx2_nix_mac_addr_del(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct cgx_mac_addr_del_req *req;
	int rc;

	if (otx2_dev_is_vf_or_sdp(dev))
		return;

	req = otx2_mbox_alloc_msg_cgx_mac_addr_del(mbox);
	req->index = index;

	rc = otx2_mbox_process(mbox);
	if (rc)
		otx2_err("Failed to delete mac address, rc=%d", rc);
}

int
otx2_nix_mac_addr_set(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_set_mac_addr *req;
	int rc;

	req = otx2_mbox_alloc_msg_nix_set_mac_addr(mbox);
	otx2_mbox_memcpy(req->mac_addr, addr->addr_bytes, RTE_ETHER_ADDR_LEN);

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Failed to set mac address, rc=%d", rc);
		goto done;
	}

	otx2_mbox_memcpy(dev->mac_addr, addr->addr_bytes, RTE_ETHER_ADDR_LEN);

	/* Install the same entry into CGX DMAC filter table too. */
	otx2_cgx_mac_addr_set(eth_dev, addr);

done:
	return rc;
}

int
otx2_nix_mac_addr_get(struct rte_eth_dev *eth_dev, uint8_t *addr)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_get_mac_addr_rsp *rsp;
	int rc;

	otx2_mbox_alloc_msg_nix_get_mac_addr(mbox);
	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp);
	if (rc) {
		otx2_err("Failed to get mac address, rc=%d", rc);
		goto done;
	}

	otx2_mbox_memcpy(addr, rsp->mac_addr, RTE_ETHER_ADDR_LEN);

done:
	return rc;
}
