/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <stdint.h>

#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_rawdev.h>

#include "cnxk_bphy_cgx.h"
#include "rte_pmd_bphy.h"

static int
cnxk_bphy_cgx_link_cond(uint16_t dev_id, unsigned int queue, int cond)
{
	struct cnxk_bphy_cgx_msg_link_info link_info;
	int tries = 10, ret;

	do {
		ret = rte_pmd_bphy_cgx_get_link_info(dev_id, queue, &link_info);
		if (ret)
			return ret;

		if (link_info.link_up == cond)
			break;

		rte_delay_ms(500);
	} while (--tries);

	if (tries)
		return !!cond;

	return -ETIMEDOUT;
}

int
cnxk_bphy_cgx_dev_selftest(uint16_t dev_id)
{
	struct cnxk_bphy_cgx_msg_set_link_state link_state = { };
	unsigned int queues, i;
	int ret;

	queues = rte_rawdev_queue_count(dev_id);
	if (queues == 0)
		return -ENODEV;

	ret = rte_rawdev_start(dev_id);
	if (ret)
		return ret;

	for (i = 0; i < queues; i++) {
		enum cnxk_bphy_cgx_eth_link_fec fec;
		unsigned int descs;

		ret = rte_rawdev_queue_conf_get(dev_id, i, &descs,
						sizeof(descs));
		if (ret)
			break;
		if (descs != 1) {
			CNXK_BPHY_LOG(ERR, "Wrong number of descs reported");
			ret = -ENODEV;
			break;
		}

		CNXK_BPHY_LOG(INFO, "Testing queue %d", i);

		ret = rte_pmd_bphy_cgx_stop_rxtx(dev_id, i);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to stop rx/tx");
			break;
		}

		ret = rte_pmd_bphy_cgx_start_rxtx(dev_id, i);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to start rx/tx");
			break;
		}

		link_state.state = false;
		ret = rte_pmd_bphy_cgx_set_link_state(dev_id, i, &link_state);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to set link down");
			break;
		}

		ret = cnxk_bphy_cgx_link_cond(dev_id, i, 0);
		if (ret != 0)
			CNXK_BPHY_LOG(ERR, "Timed out waiting for a link down");

		link_state.state = true;
		link_state.timeout = 1500;
		link_state.rx_tx_dis = true;
		ret = rte_pmd_bphy_cgx_set_link_state(dev_id, i, &link_state);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to set link up");
			break;
		}

		ret = cnxk_bphy_cgx_link_cond(dev_id, i, 1);
		if (ret != 1)
			CNXK_BPHY_LOG(ERR, "Timed out waiting for a link up");

		ret = rte_pmd_bphy_cgx_intlbk_enable(dev_id, i);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to enable internal lbk");
			break;
		}

		ret = rte_pmd_bphy_cgx_intlbk_disable(dev_id, i);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to disable internal lbk");
			break;
		}

		ret = rte_pmd_bphy_cgx_ptp_rx_enable(dev_id, i);
		/* ptp not available on RPM */
		if (ret < 0 && ret != -ENOTSUP) {
			CNXK_BPHY_LOG(ERR, "Failed to enable ptp");
			break;
		}
		ret = 0;

		ret = rte_pmd_bphy_cgx_ptp_rx_disable(dev_id, i);
		/* ptp not available on RPM */
		if (ret < 0 && ret != -ENOTSUP) {
			CNXK_BPHY_LOG(ERR, "Failed to disable ptp");
			break;
		}
		ret = 0;

		ret = rte_pmd_bphy_cgx_get_supported_fec(dev_id, i, &fec);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to get supported FEC");
			break;
		}

		ret = rte_pmd_bphy_cgx_set_fec(dev_id, i, fec);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to set FEC to %d", fec);
			break;
		}

		fec = CNXK_BPHY_CGX_ETH_LINK_FEC_NONE;
		ret = rte_pmd_bphy_cgx_set_fec(dev_id, i, fec);
		if (ret) {
			CNXK_BPHY_LOG(ERR, "Failed to disable FEC");
			break;
		}
	}

	rte_rawdev_stop(dev_id);

	return ret;
}
