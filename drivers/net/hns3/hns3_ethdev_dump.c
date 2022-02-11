/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 HiSilicon Limited
 */

#include <rte_kvargs.h>
#include <rte_bus_pci.h>
#include <ethdev_pci.h>
#include <rte_pci.h>

#include "hns3_common.h"
#include "hns3_logs.h"
#include "hns3_regs.h"
#include "hns3_rxtx.h"

static const char *
get_adapter_state_name(uint32_t state)
{
	static const char * const state_name[] = {
		"UNINITIALIZED",
		"INITIALIZED",
		"CONFIGURING",
		"CONFIGURED",
		"STARTING",
		"STARTED",
		"STOPPING",
		"CLOSING",
		"CLOSED",
		"REMOVED",
		"NSTATES"
	};

	if (state < RTE_DIM(state_name))
		return state_name[state];
	else
		return "unknown";
}

static const char *
get_io_func_hint_name(uint32_t hint)
{
	switch (hint) {
	case HNS3_IO_FUNC_HINT_NONE:
		return "none";
	case HNS3_IO_FUNC_HINT_VEC:
		return "vec";
	case HNS3_IO_FUNC_HINT_SVE:
		return "sve";
	case HNS3_IO_FUNC_HINT_SIMPLE:
		return "simple";
	case HNS3_IO_FUNC_HINT_COMMON:
		return "common";
	default:
		return "unknown";
	}
}

static void
get_device_basic_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	fprintf(file,
		"  - Device Base Info:\n"
		"\t  -- name: %s\n"
		"\t  -- adapter_state=%s\n"
		"\t  -- nb_rx_queues=%u nb_tx_queues=%u\n"
		"\t  -- total_tqps_num=%u tqps_num=%u intr_tqps_num=%u\n"
		"\t  -- rss_size_max=%u alloc_rss_size=%u tx_qnum_per_tc=%u\n"
		"\t  -- min_tx_pkt_len=%u intr_mapping_mode=%u vlan_mode=%u\n"
		"\t  -- tso_mode=%u max_non_tso_bd_num=%u\n"
		"\t  -- max_tm_rate=%u Mbps\n"
		"\t  -- set link down: %s\n"
		"\t  -- rx_func_hint=%s tx_func_hint=%s\n"
		"\t  -- dev_flags: lsc=%d\n"
		"\t  -- intr_conf: lsc=%u rxq=%u\n",
		dev->data->name,
		get_adapter_state_name(hw->adapter_state),
		dev->data->nb_rx_queues, dev->data->nb_tx_queues,
		hw->total_tqps_num, hw->tqps_num, hw->intr_tqps_num,
		hw->rss_size_max, hw->alloc_rss_size, hw->tx_qnum_per_tc,
		hw->min_tx_pkt_len, hw->intr.mapping_mode, hw->vlan_mode,
		hw->tso_mode, hw->max_non_tso_bd_num,
		hw->max_tm_rate,
		hw->set_link_down ? "Yes" : "No",
		get_io_func_hint_name(hns->rx_func_hint),
		get_io_func_hint_name(hns->tx_func_hint),
		!!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC),
		dev->data->dev_conf.intr_conf.lsc,
		dev->data->dev_conf.intr_conf.rxq);
}

int
hns3_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file)
{
	get_device_basic_info(file, dev);

	return 0;
}
