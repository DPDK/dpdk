/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_malloc.h>
#include <arpa/inet.h>

#include "sxe2_common_log.h"
#include "sxe2_ethdev.h"
#include "sxe2_dump.h"
#include "sxe2_stats.h"

static void
sxe2_dump_dev_feature_capability(FILE *file, struct rte_eth_dev *dev)
{
	uint32_t i;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	const struct {
		uint32_t cap_bit;
		const char *name;
	} caps_name[] = {
		{SXE2_DEV_CAPS_OFFLOAD_L2, "L2"},
		{SXE2_DEV_CAPS_OFFLOAD_VLAN, "VLAN"},
		{SXE2_DEV_CAPS_OFFLOAD_IPSEC, "IPSEC"},
		{SXE2_DEV_CAPS_OFFLOAD_RSS, "RSS"},
		{SXE2_DEV_CAPS_OFFLOAD_FNAV, "FNAV"},
		{SXE2_DEV_CAPS_OFFLOAD_TM, "TM"},
		{SXE2_DEV_CAPS_OFFLOAD_PTP, "PTP"},
	};
	if (adapter->is_dev_repr)
		goto l_end;

	fprintf(file, "  - Dev Capability:\n");
	for (i = 0; i < RTE_DIM(caps_name); i++) {
		fprintf(file, "\t  -- support %s: %s\n", caps_name[i].name,
			(adapter->cap_flags & caps_name[i].cap_bit) ? "Yes" :
									 "No");
	}
l_end:
	return;
}

static void
sxe2_dump_device_basic_info(FILE *file, struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	fprintf(file,
		"  - Device Base Info:\n"
		"\t  -- name: %s\n"
		"\t  -- pf_idx: %u port_idx: %u\n"
		"\t  -- tx_mode_flags: 0x%x rx_mode_flags: 0x%x\n"
		"\t  -- flow_isolate_cfg: 0x%x flow_isolated: 0x%x\n"
		"\t  -- dev_type: 0x%x is_switchdev: 0x%x\n"
		"\t  -- is_dev_repr: 0x%x dev_port_id: 0x%x\n"
		"\t  -- dev_flags: 0x%x\n"
		"\t  -- intr_conf lsc: %u rxq: %u rmv: %u\n",
		dev->data->name,
		adapter->pf_idx, adapter->port_idx,
		adapter->tx_mode_flags, adapter->rx_mode_flags,
		adapter->flow_isolate_cfg, adapter->flow_isolated,
		adapter->dev_type, adapter->switchdev_info.is_switchdev,
		adapter->is_dev_repr, adapter->dev_port_id,
		dev->data->dev_flags,
		dev->data->dev_conf.intr_conf.lsc,
		dev->data->dev_conf.intr_conf.rxq,
		dev->data->dev_conf.intr_conf.rmv);
}

static void
sxe2_dump_dev_args_info(FILE *file, struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (adapter->is_dev_repr)
		goto l_end;

	fprintf(file,
		"  - Device Args Info:\n"
		"\t  -- no_sched_mode: %s\n"
		"\t  -- flow-duplicate-pattern: %u\n"
		"\t  -- fnav-stat-type: %u\n"
		"\t  -- sched_layer_mode: %u\n"
		"\t  -- rx_low_latency: %s\n"
		"\t  -- function-flow-direct: %s\n",
		adapter->devargs.no_sched_mode ? "On" : "Off",
		adapter->devargs.flow_dup_pattern_mode,
		adapter->devargs.fnav_stat_type,
		adapter->devargs.sched_layer_mode,
		adapter->devargs.rx_low_latency ? "On" : "Off",
		adapter->devargs.func_flow_direct_en ? "On" : "Off");
l_end:
	return;
}

static void sxe2_dump_filter_info(FILE *file, struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_mac_filter *mac_entry;
	struct sxe2_mac_filter *next_mac_entry;
	struct sxe2_vlan_filter *vlan_entry;
	struct sxe2_vlan_filter *next_vlan_entry;

	if (adapter->is_dev_repr)
		goto l_end;

	fprintf(file,
		"  - Device Filter Info:\n"
		"\t  -- cur_promisc:0x%x hw_promisc:0x%x\n"
		"\t  -- unicast_num: %u multicast_num: %u\n"
		"\t  -- vlan_num: %u filter_on: %u hw_filter_on: %u\n"
		"\t  -- vlan max_cnt: %u cnt: %u\n"
		"\t  -- tpid: 0x%x vid: 0x%x\n"
		"\t  -- vlan_outer_insert: 0x%x vlan_outer_strip: 0x%x\n"
		"\t  -- vlan_inner_insert: 0x%x vlan_inner_strip: 0x%x\n",
		adapter->filter_ctxt.cur_promisc_flags,
		adapter->filter_ctxt.hw_promisc_flags,
		adapter->filter_ctxt.uc_num,
		adapter->filter_ctxt.mc_num,
		adapter->filter_ctxt.vlan_num,
		adapter->filter_ctxt.vlan_info.filter_on,
		adapter->filter_ctxt.vlan_info.hw_filter_on,
		adapter->filter_ctxt.vlan_info.max_cnt,
		adapter->filter_ctxt.vlan_info.cnt,
		adapter->filter_ctxt.vlan_info.tpid,
		adapter->filter_ctxt.vlan_info.vid,
		adapter->filter_ctxt.vlan_info.outer_insert,
		adapter->filter_ctxt.vlan_info.outer_strip,
		adapter->filter_ctxt.vlan_info.inner_insert,
		adapter->filter_ctxt.vlan_info.inner_strip);

	if (adapter->filter_ctxt.uc_num > 0) {
		fprintf(file,
			"\t  -- Unicast entry:\n");
		RTE_TAILQ_FOREACH_SAFE(mac_entry, &adapter->filter_ctxt.uc_list, next,
				       next_mac_entry) {
			fprintf(file,
				"\t  -- addr: %02x:%02x:%02x:%02x:%02x:%02x hw status:%u "
				"default:%u\n",
				mac_entry->mac_addr.addr_bytes[0],
				mac_entry->mac_addr.addr_bytes[1],
				mac_entry->mac_addr.addr_bytes[2],
				mac_entry->mac_addr.addr_bytes[3],
				mac_entry->mac_addr.addr_bytes[4],
				mac_entry->mac_addr.addr_bytes[5],
				mac_entry->hw_config,
				mac_entry->default_config);
		}
	}

	if (adapter->filter_ctxt.mc_num > 0) {
		fprintf(file,
			"\t  -- Multicast entry:\n");
		RTE_TAILQ_FOREACH_SAFE(mac_entry, &adapter->filter_ctxt.mc_list,
				       next, next_mac_entry) {
			fprintf(file,
				"\t  -- addr: %02x:%02x:%02x:%02x:%02x:%02x "
				"hw status:%u default:%u\n",
				mac_entry->mac_addr.addr_bytes[0],
				mac_entry->mac_addr.addr_bytes[1],
				mac_entry->mac_addr.addr_bytes[2],
				mac_entry->mac_addr.addr_bytes[3],
				mac_entry->mac_addr.addr_bytes[4],
				mac_entry->mac_addr.addr_bytes[5],
				mac_entry->hw_config,
				mac_entry->default_config);
		}
	}

	if (adapter->filter_ctxt.vlan_num > 0) {
		fprintf(file,
			"\t  -- Vlan entry:\n");
		RTE_TAILQ_FOREACH_SAFE(vlan_entry, &adapter->filter_ctxt.vlan_list,
			next, next_vlan_entry) {
			fprintf(file,
				"\t  -- vlan tpid:0x%04x vid:0x%04x prio:%d "
				"hw status:%u default:%u\n",
				vlan_entry->vlan_info.tpid,
				vlan_entry->vlan_info.vid,
				vlan_entry->vlan_info.prio,
				vlan_entry->hw_config,
				vlan_entry->default_config);
		}
	}
l_end:
	return;
}

static const char *sxe2_vsi_id_str(uint16_t vsi_id, char *buf, size_t len)
{
	if (vsi_id == SXE2_INVALID_VSI_ID)
		return "NA";

	snprintf(buf, len, "%u", vsi_id);
	return buf;
}

static void
sxe2_dump_switchdev_info(FILE *file, struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	uint32_t idx;
	char k_vsi_buf[16];
	char u_vsi_buf[16];

	if (adapter->is_dev_repr && adapter->repr_priv_data) {
		fprintf(file,
			"  - Reprenstor Info:\n"
			"\t  -- repr_id: %u\n"
			"\t  -- repr_q_id: %u\n"
			"\t  -- repr_pf_id: %u\n"
			"\t  -- repr_vf_id: %u\n"
			"\t  -- repr_vf_vsi_id: %u\n"
			"\t  -- repr_vf_k_vsi_id: %s\n"
			"\t  -- repr_vf_u_vsi_id: %s\n",
			adapter->repr_priv_data->repr_id,
			adapter->repr_priv_data->repr_q_id,
			adapter->repr_priv_data->repr_pf_id,
			adapter->repr_priv_data->repr_vf_id,
			adapter->repr_priv_data->repr_vf_vsi_id,
			sxe2_vsi_id_str(adapter->repr_priv_data->repr_vf_k_vsi_id,
					k_vsi_buf, sizeof(k_vsi_buf)),
			sxe2_vsi_id_str(adapter->repr_priv_data->repr_vf_u_vsi_id,
					u_vsi_buf, sizeof(u_vsi_buf)));
		goto l_end;
	}
	if (adapter->switchdev_info.is_switchdev) {
		fprintf(file,
			"  - Switchdev Info:\n"
			"\t  -- primary:0x%x\n"
			"\t  -- representor: 0x%x\n"
			"\t  -- port_name_type: 0x%x\n"
			"\t  -- nb_vf: %u nb_repr_vf: %u\n",
			adapter->switchdev_info.primary,
			adapter->switchdev_info.representor,
			adapter->switchdev_info.port_name_type,
			adapter->repr_ctxt.nb_vf,
			adapter->repr_ctxt.nb_repr_vf);
		if (adapter->repr_ctxt.nb_vf > 0) {
			fprintf(file,
				"\t  -- vf entry:\n");
			for (idx = 0; idx < adapter->repr_ctxt.nb_vf; idx++) {
				fprintf(file,
					"\t  -- func_id:%u vsi_type:%u kernel_vsi_id:%u dpdk_vsi_id:%u\n",
					adapter->repr_ctxt.repr_vf_id[idx].func_id,
					adapter->repr_ctxt.repr_vf_id[idx].vsi_type,
					adapter->repr_ctxt.repr_vf_id[idx].kernel_vsi_id,
					adapter->repr_ctxt.repr_vf_id[idx].dpdk_vsi_id);
			}
		}
	}

l_end:
	return;
}

int32_t sxe2_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file)
{
	char *buf = NULL;
	size_t size = 0;
	FILE *str;
	int32_t ret = -1;

	str = open_memstream(&buf, &size);
	if (!str) {
		PMD_LOG_ERR(DRV, "fopen fail.");
		goto l_end;
	}

	sxe2_dump_dev_feature_capability(str, dev);
	sxe2_dump_device_basic_info(str, dev);
	sxe2_dump_dev_args_info(str, dev);
	sxe2_dump_filter_info(str, dev);
	sxe2_dump_switchdev_info(str, dev);

	(void)fflush(str);

	(void)fwrite(buf, 1, size, file);
	(void)fflush(file);

	ret = 0;

	(void)fclose(str);
	free(buf);
l_end:
	return ret;
}
