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
			RTE_LOG(ERR, PMD, "Wrong number of descs reported\n");
			ret = -ENODEV;
			break;
		}

		RTE_LOG(INFO, PMD, "Testing queue %d\n", i);

		ret = rte_pmd_bphy_cgx_stop_rxtx(dev_id, i);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to stop rx/tx\n");
			break;
		}

		ret = rte_pmd_bphy_cgx_start_rxtx(dev_id, i);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to start rx/tx\n");
			break;
		}

		ret = rte_pmd_bphy_cgx_set_link_state(dev_id, i, false);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to set link down\n");
			break;
		}

		ret = cnxk_bphy_cgx_link_cond(dev_id, i, 0);
		if (ret != 0)
			RTE_LOG(ERR, PMD,
				"Timed out waiting for a link down\n");

		ret = rte_pmd_bphy_cgx_set_link_state(dev_id, i, true);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to set link up\n");
			break;
		}

		ret = cnxk_bphy_cgx_link_cond(dev_id, i, 1);
		if (ret != 1)
			RTE_LOG(ERR, PMD, "Timed out waiting for a link up\n");

		ret = rte_pmd_bphy_cgx_intlbk_enable(dev_id, i);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to enable internal lbk\n");
			break;
		}

		ret = rte_pmd_bphy_cgx_intlbk_disable(dev_id, i);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to disable internal lbk\n");
			break;
		}

		ret = rte_pmd_bphy_cgx_ptp_rx_enable(dev_id, i);
		/* ptp not available on RPM */
		if (ret < 0 && ret != -ENOTSUP) {
			RTE_LOG(ERR, PMD, "Failed to enable ptp\n");
			break;
		}
		ret = 0;

		ret = rte_pmd_bphy_cgx_ptp_rx_disable(dev_id, i);
		/* ptp not available on RPM */
		if (ret < 0 && ret != -ENOTSUP) {
			RTE_LOG(ERR, PMD, "Failed to disable ptp\n");
			break;
		}
		ret = 0;

		ret = rte_pmd_bphy_cgx_get_supported_fec(dev_id, i, &fec);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to get supported FEC\n");
			break;
		}

		ret = rte_pmd_bphy_cgx_set_fec(dev_id, i, fec);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to set FEC to %d\n", fec);
			break;
		}

		fec = CNXK_BPHY_CGX_ETH_LINK_FEC_NONE;
		ret = rte_pmd_bphy_cgx_set_fec(dev_id, i, fec);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to disable FEC\n");
			break;
		}
	}

	rte_rawdev_stop(dev_id);

	return ret;
}
