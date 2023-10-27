/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_common.h"

#include "nfp_common_log.h"

/*
 * This is used by the reconfig protocol. It sets the maximum time waiting in
 * milliseconds before a reconfig timeout happens.
 */
#define NFP_NET_POLL_TIMEOUT    5000

int
nfp_reconfig_real(struct nfp_hw *hw,
		uint32_t update)
{
	uint32_t cnt;
	uint32_t new;
	struct timespec wait;

	PMD_DRV_LOG(DEBUG, "Writing to the configuration queue (%p)...",
			hw->qcp_cfg);

	if (hw->qcp_cfg == NULL) {
		PMD_DRV_LOG(ERR, "Bad configuration queue pointer");
		return -ENXIO;
	}

	nfp_qcp_ptr_add(hw->qcp_cfg, NFP_QCP_WRITE_PTR, 1);

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000; /* 1ms */

	PMD_DRV_LOG(DEBUG, "Polling for update ack...");

	/* Poll update field, waiting for NFP to ack the config */
	for (cnt = 0; ; cnt++) {
		new = nn_cfg_readl(hw, NFP_NET_CFG_UPDATE);
		if (new == 0)
			break;

		if ((new & NFP_NET_CFG_UPDATE_ERR) != 0) {
			PMD_DRV_LOG(ERR, "Reconfig error: %#08x", new);
			return -1;
		}

		if (cnt >= NFP_NET_POLL_TIMEOUT) {
			PMD_DRV_LOG(ERR, "Reconfig timeout for %#08x after %u ms",
					update, cnt);
			return -EIO;
		}

		nanosleep(&wait, 0); /* waiting for a 1ms */
	}

	PMD_DRV_LOG(DEBUG, "Ack DONE");
	return 0;
}

/**
 * Reconfigure the NIC.
 *
 * Write the update word to the BAR and ping the reconfig queue. Then poll
 * until the firmware has acknowledged the update by zeroing the update word.
 *
 * @param hw
 *   Device to reconfigure.
 * @param ctrl
 *   The value for the ctrl field in the BAR config.
 * @param update
 *   The value for the update field in the BAR config.
 *
 * @return
 *   - (0) if OK to reconfigure the device.
 *   - (-EIO) if I/O err and fail to reconfigure the device.
 */
int
nfp_reconfig(struct nfp_hw *hw,
		uint32_t ctrl,
		uint32_t update)
{
	int ret;

	rte_spinlock_lock(&hw->reconfig_lock);

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, ctrl);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, update);

	rte_wmb();

	ret = nfp_reconfig_real(hw, update);

	rte_spinlock_unlock(&hw->reconfig_lock);

	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Error nfp reconfig: ctrl=%#08x update=%#08x",
				ctrl, update);
		return -EIO;
	}

	return 0;
}

/**
 * Reconfigure the NIC for the extend ctrl BAR.
 *
 * Write the update word to the BAR and ping the reconfig queue. Then poll
 * until the firmware has acknowledged the update by zeroing the update word.
 *
 * @param hw
 *   Device to reconfigure.
 * @param ctrl_ext
 *   The value for the first word of extend ctrl field in the BAR config.
 * @param update
 *   The value for the update field in the BAR config.
 *
 * @return
 *   - (0) if OK to reconfigure the device.
 *   - (-EIO) if I/O err and fail to reconfigure the device.
 */
int
nfp_ext_reconfig(struct nfp_hw *hw,
		uint32_t ctrl_ext,
		uint32_t update)
{
	int ret;

	rte_spinlock_lock(&hw->reconfig_lock);

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL_WORD1, ctrl_ext);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, update);

	rte_wmb();

	ret = nfp_reconfig_real(hw, update);

	rte_spinlock_unlock(&hw->reconfig_lock);

	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Error nfp ext reconfig: ctrl_ext=%#08x update=%#08x",
				ctrl_ext, update);
		return -EIO;
	}

	return 0;
}

void
nfp_read_mac(struct nfp_hw *hw)
{
	uint32_t tmp;

	tmp = rte_be_to_cpu_32(nn_cfg_readl(hw, NFP_NET_CFG_MACADDR));
	memcpy(&hw->mac_addr.addr_bytes[0], &tmp, 4);

	tmp = rte_be_to_cpu_32(nn_cfg_readl(hw, NFP_NET_CFG_MACADDR + 4));
	memcpy(&hw->mac_addr.addr_bytes[4], &tmp, 2);
}

void
nfp_write_mac(struct nfp_hw *hw,
		uint8_t *mac)
{
	uint32_t mac0;
	uint16_t mac1;

	mac0 = *(uint32_t *)mac;
	nn_writel(rte_cpu_to_be_32(mac0), hw->ctrl_bar + NFP_NET_CFG_MACADDR);

	mac += 4;
	mac1 = *(uint16_t *)mac;
	nn_writew(rte_cpu_to_be_16(mac1),
			hw->ctrl_bar + NFP_NET_CFG_MACADDR + 6);
}

void
nfp_enable_queues(struct nfp_hw *hw,
		uint16_t nb_rx_queues,
		uint16_t nb_tx_queues)
{
	int i;
	uint64_t enabled_queues;

	/* Enabling the required TX queues in the device */
	enabled_queues = 0;
	for (i = 0; i < nb_tx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, enabled_queues);

	/* Enabling the required RX queues in the device */
	enabled_queues = 0;
	for (i = 0; i < nb_rx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, enabled_queues);
}

void
nfp_disable_queues(struct nfp_hw *hw)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, 0);
	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, 0);

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN |
			NFP_NET_CFG_UPDATE_RING |
			NFP_NET_CFG_UPDATE_MSIX;

	if ((hw->cap & NFP_NET_CFG_CTRL_RINGCFG) != 0)
		new_ctrl &= ~NFP_NET_CFG_CTRL_RINGCFG;

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_reconfig(hw, new_ctrl, update);
	if (ret < 0)
		return;

	hw->ctrl = new_ctrl;
}
