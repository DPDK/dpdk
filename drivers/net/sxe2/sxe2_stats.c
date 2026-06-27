/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_eal.h>

#include "sxe2_ethdev.h"
#include "sxe2_vsi.h"
#include "sxe2_common_log.h"
#include "sxe2_stats.h"
#include "sxe2_queue.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_mp.h"

#define SXE2_XSTAT_CNT_PF RTE_DIM(sxe2_xstats_field_pf)
#define SXE2_XSTAT_CNT_VF RTE_DIM(sxe2_xstats_field_vf)

static struct sxe2_stats_field sxe2_xstats_field_pf[] = {
	{"rx_qblock_drop", offsetof(struct sxe2_stats, rx_qblock_drop)},
	{"rx_out_of_buffer", offsetof(struct sxe2_stats, rx_out_of_buffer)},
	{"tx_packets_phy", offsetof(struct sxe2_stats, tx_frame_good)},
	{"rx_packets_phy", offsetof(struct sxe2_stats, rx_frame_good)},
	{"rx_crc_errors_phy", offsetof(struct sxe2_stats, rx_crc_errors)},
	{"tx_bytes_phy", offsetof(struct sxe2_stats, tx_bytes_good)},
	{"rx_bytes_phy", offsetof(struct sxe2_stats, rx_bytes_good)},
	{"tx_multicast_phy", offsetof(struct sxe2_stats, tx_multicast_good)},
	{"tx_broadcast_phy", offsetof(struct sxe2_stats, tx_broadcast_good)},
	{"rx_multicast_phy", offsetof(struct sxe2_stats, rx_multicast_good)},
	{"rx_broadcast_phy", offsetof(struct sxe2_stats, rx_broadcast_good)},
	{"rx_in_range_len_errs_phy", offsetof(struct sxe2_stats, rx_len_errors)},
	{"rx_out_of_range_len_phy", offsetof(struct sxe2_stats, rx_out_of_range_errors)},
	{"rx_oversize_pkts_phy", offsetof(struct sxe2_stats, rx_oversize_pkts_phy)},
	{"rx_symbol_err_phy", offsetof(struct sxe2_stats, rx_symbol_err)},
	{"rx_pause_ctrl_phy", offsetof(struct sxe2_stats, rx_pause_frame)},
	{"tx_pause_ctrl_phy", offsetof(struct sxe2_stats, tx_pause_frame)},
	{"rx_discards_phy", offsetof(struct sxe2_stats, rx_discards_phy)},
	{"rx_discards_ips_phy", offsetof(struct sxe2_stats, rx_discards_ips_phy)},
	{"tx_dropped_link_down_phy", offsetof(struct sxe2_stats, tx_dropped_link_down)},
	{"rx_undersize_pkts_phy", offsetof(struct sxe2_stats, rx_undersize_good)},
	{"rx_fragments_phy", offsetof(struct sxe2_stats, rx_runt_error)},
	{"tx_bytes_all_phy", offsetof(struct sxe2_stats, tx_bytes_good_bad)},
	{"tx_packets_all_phy", offsetof(struct sxe2_stats, tx_frame_good_bad)},
	{"rx_jabbers_phy", offsetof(struct sxe2_stats, rx_jabbers)},
	{"rx_64_bytes_phy", offsetof(struct sxe2_stats, rx_size_64)},
	{"rx_65_to_127_bytes_phy", offsetof(struct sxe2_stats, rx_size_65_127)},
	{"rx_128_to_255_bytes_phy", offsetof(struct sxe2_stats, rx_size_128_255)},
	{"rx_256_to_511_bytes_phy", offsetof(struct sxe2_stats, rx_size_256_511)},
	{"rx_512_to_1023_bytes_phy", offsetof(struct sxe2_stats, rx_size_512_1023)},
	{"rx_1024_to_1522_bytes_phy", offsetof(struct sxe2_stats, rx_size_1024_1522)},
	{"rx_1523_to_max_bytes_phy", offsetof(struct sxe2_stats, rx_size_1523_max)},
	{"rx_pcs_symbol_err_phy", offsetof(struct sxe2_stats, rx_pcs_symbol_err_phy)},
	{"rx_corrected_bits_phy", offsetof(struct sxe2_stats, rx_corrected_bits_phy)},
	{"rx_err_lane_0_phy", offsetof(struct sxe2_stats, rx_err_lane_0_phy)},
	{"rx_err_lane_1_phy", offsetof(struct sxe2_stats, rx_err_lane_1_phy)},
	{"rx_err_lane_2_phy", offsetof(struct sxe2_stats, rx_err_lane_2_phy)},
	{"rx_err_lane_3_phy", offsetof(struct sxe2_stats, rx_err_lane_3_phy)},
	{"rx_illegal_bytes_phy", offsetof(struct sxe2_stats, rx_illegal_bytes)},
	{"rx_oversize_good_phy", offsetof(struct sxe2_stats, rx_oversize_good)},
	{"tx_unicast_all_phy", offsetof(struct sxe2_stats, tx_unicast)},
	{"tx_broadcast_all_phy", offsetof(struct sxe2_stats, tx_broadcast)},
	{"tx_multicast_all_phy", offsetof(struct sxe2_stats, tx_multicast)},
	{"tx_vlan_packets_good_phy", offsetof(struct sxe2_stats, tx_vlan_packet_good)},
	{"tx_64_bytes_phy", offsetof(struct sxe2_stats, tx_size_64)},
	{"tx_65_to_127_bytes_phy", offsetof(struct sxe2_stats, tx_size_65_127)},
	{"tx_128_to_255_bytes_phy", offsetof(struct sxe2_stats, tx_size_128_255)},
	{"tx_256_to_511_bytes_phy", offsetof(struct sxe2_stats, tx_size_256_511)},
	{"tx_512_to_1023_bytes_phy", offsetof(struct sxe2_stats, tx_size_512_1023)},
	{"tx_1024_to_1522_bytes_phy", offsetof(struct sxe2_stats, tx_size_1024_1522)},
	{"tx_1523_to_max_bytes_phy", offsetof(struct sxe2_stats, tx_size_1523_max)},
	{"tx_underflow_error_phy", offsetof(struct sxe2_stats, tx_underflow_error)},
	{"rx_bytes_all_phy", offsetof(struct sxe2_stats, rx_byte_good_bad)},
	{"rx_packets_all_phy", offsetof(struct sxe2_stats, rx_frame_good_bad)},
	{"rx_unicast_phy", offsetof(struct sxe2_stats, rx_unicast_good)},
	{"rx_vlan_packets_phy", offsetof(struct sxe2_stats, rx_vlan_packets)},

	{"rx_vport_bytes",             offsetof(struct sxe2_stats, rx_vsi_bytes)},
	{"rx_vport_unicast_packets",   offsetof(struct sxe2_stats, rx_vsi_unicast_packets)},
	{"rx_vport_broadcast_packets", offsetof(struct sxe2_stats, rx_vsi_broadcast_packets)},
	{"rx_vport_multicast_packets", offsetof(struct sxe2_stats, rx_vsi_multicast_packets)},
	{"rx_sw_unicast_packets",      offsetof(struct sxe2_stats, rx_sw_unicast_packets)},
	{"rx_sw_broadcast_packets",    offsetof(struct sxe2_stats, rx_sw_broadcast_packets)},
	{"rx_sw_multicast_packets",    offsetof(struct sxe2_stats, rx_sw_multicast_packets)},
	{"rx_sw_drop_packets",         offsetof(struct sxe2_stats, rx_sw_drop_packets)},
	{"rx_sw_drop_bytes",           offsetof(struct sxe2_stats, rx_sw_drop_bytes)},

	{"tx_vport_bytes",             offsetof(struct sxe2_stats, tx_vsi_bytes)},
	{"tx_vport_unicast_packets",   offsetof(struct sxe2_stats, tx_vsi_unicast_packets)},
	{"tx_vport_broadcast_packets", offsetof(struct sxe2_stats, tx_vsi_broadcast_packets)},
	{"tx_vport_multicast_packets", offsetof(struct sxe2_stats, tx_vsi_multicast_packets)},
};

static struct sxe2_stats_field sxe2_xstats_field_vf[] = {
	{"rx_vport_bytes",             offsetof(struct sxe2_stats, rx_vsi_bytes)},
	{"rx_vport_unicast_packets",   offsetof(struct sxe2_stats, rx_vsi_unicast_packets)},
	{"rx_vport_broadcast_packets", offsetof(struct sxe2_stats, rx_vsi_broadcast_packets)},
	{"rx_vport_multicast_packets", offsetof(struct sxe2_stats, rx_vsi_multicast_packets)},
	{"rx_sw_unicast_packets",      offsetof(struct sxe2_stats, rx_sw_unicast_packets)},
	{"rx_sw_broadcast_packets",    offsetof(struct sxe2_stats, rx_sw_broadcast_packets)},
	{"rx_sw_multicast_packets",    offsetof(struct sxe2_stats, rx_sw_multicast_packets)},
	{"rx_sw_drop_packets",         offsetof(struct sxe2_stats, rx_sw_drop_packets)},
	{"rx_sw_drop_bytes",           offsetof(struct sxe2_stats, rx_sw_drop_bytes)},

	{"tx_vport_bytes",             offsetof(struct sxe2_stats, tx_vsi_bytes)},
	{"tx_vport_unicast_packets",   offsetof(struct sxe2_stats, tx_vsi_unicast_packets)},
	{"tx_vport_broadcast_packets", offsetof(struct sxe2_stats, tx_vsi_broadcast_packets)},
	{"tx_vport_multicast_packets", offsetof(struct sxe2_stats, tx_vsi_multicast_packets)},
};

static int32_t sxe2_xstat_pf_offset_get(uint32_t id, uint32_t *offset)
{
	int32_t ret  = 0;
	uint32_t size = SXE2_XSTAT_CNT_PF;

	if (id < size) {
		*offset = sxe2_xstats_field_pf[id].offset;
	} else {
		ret = -EINVAL;
		PMD_LOG_ERR(DRV, "invalid id:%u exceed stats size cnt:%u", id, size);
	}
	return ret;
}

static int32_t sxe2_xstat_vf_offset_get(uint32_t id, uint32_t *offset)
{
	int32_t ret  = 0;
	uint32_t size = SXE2_XSTAT_CNT_VF;

	if (id < size) {
		*offset = sxe2_xstats_field_vf[id].offset;
	} else {
		ret = -EINVAL;
		PMD_LOG_ERR(DRV, "invalid id:%u exceed stats size cnt:%u", id, size);
	}
	return ret;
}

static int32_t sxe2_mac_hw_stats_get_update(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	ret = sxe2_drv_get_mac_stats(adapter);
	if (ret) {
		PMD_LOG_ERR(DRV, "get mac stats failed, ret:%d.", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_vsi_hw_stats_get_update(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	ret = sxe2_drv_get_vsi_stats(adapter);
	if (ret) {
		PMD_LOG_ERR(DRV, "get vsi stats failed, ret:%d.", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_vsi_sw_stats_get_update(struct sxe2_adapter *adapter)
{
	int32_t ret  = 0;
	struct sxe2_vsi          *vsi      = adapter->vsi_ctxt.main_vsi;
	struct sxe2_stats        *sw_stats = &vsi->vsi_stats.vsi_sw_stats;
	struct sxe2_rx_queue     *rxq;
	uint32_t rx_queue_id;
	memset(sw_stats, 0, sizeof(struct sxe2_stats));

	for (rx_queue_id = 0; rx_queue_id < adapter->dev_info.dev_data->nb_rx_queues;
		     rx_queue_id++) {
		rxq = adapter->dev_info.dev_data->rx_queues[rx_queue_id];
		if (rxq) {
			sw_stats->ipackets += rxq->sw_stats.pkts;
			sw_stats->ierrors += rxq->sw_stats.drop_pkts;
			sw_stats->ibytes += rxq->sw_stats.bytes;

			sw_stats->rx_sw_unicast_packets += rxq->sw_stats.unicast_pkts;
			sw_stats->rx_sw_broadcast_packets += rxq->sw_stats.broadcast_pkts;
			sw_stats->rx_sw_multicast_packets += rxq->sw_stats.multicast_pkts;
			sw_stats->rx_sw_drop_packets += rxq->sw_stats.drop_pkts;
			sw_stats->rx_sw_drop_bytes += rxq->sw_stats.drop_bytes;
		}
	}

	return ret;
}

static void sxe2_stats_update(struct sxe2_adapter *adapter)
{
	struct sxe2_vsi          *vsi      = adapter->vsi_ctxt.main_vsi;
	struct sxe2_stats        *stats = &vsi->vsi_stats.stats;
	struct sxe2_stats        *hw_stats = &vsi->vsi_stats.vsi_hw_stats;
	struct sxe2_stats        *sw_stats = &vsi->vsi_stats.vsi_sw_stats;
	struct sxe2_stats        *sw_stats_prev = &vsi->vsi_stats.vsi_sw_stats_prev;
	uint8_t i = 0;

	memset(stats, 0, sizeof(struct sxe2_stats));

	stats->opackets = hw_stats->opackets;
	stats->obytes   = hw_stats->tx_vsi_bytes;
	stats->tx_vsi_bytes             = hw_stats->tx_vsi_bytes;
	stats->tx_vsi_unicast_packets   = hw_stats->tx_vsi_unicast_packets;
	stats->tx_vsi_broadcast_packets = hw_stats->tx_vsi_broadcast_packets;
	stats->tx_vsi_multicast_packets = hw_stats->tx_vsi_multicast_packets;

	stats->ierrors = sw_stats->ierrors + sw_stats_prev->ierrors;
	stats->ipackets = sw_stats->ipackets + sw_stats_prev->ipackets;
	stats->ibytes = sw_stats->ibytes + sw_stats_prev->ibytes;
	stats->rx_vsi_bytes             = hw_stats->rx_vsi_bytes;
	stats->rx_vsi_unicast_packets   = hw_stats->rx_vsi_unicast_packets;
	stats->rx_vsi_broadcast_packets = hw_stats->rx_vsi_broadcast_packets;
	stats->rx_vsi_multicast_packets = hw_stats->rx_vsi_multicast_packets;
	stats->rx_sw_unicast_packets = sw_stats->rx_sw_unicast_packets +
			sw_stats_prev->rx_sw_unicast_packets;
	stats->rx_sw_broadcast_packets = sw_stats->rx_sw_broadcast_packets +
			sw_stats_prev->rx_sw_broadcast_packets;
	stats->rx_sw_multicast_packets = sw_stats->rx_sw_multicast_packets +
			sw_stats_prev->rx_sw_multicast_packets;
	stats->rx_sw_drop_packets = sw_stats->rx_sw_drop_packets +
			sw_stats_prev->rx_sw_drop_packets;
	stats->rx_sw_drop_bytes = sw_stats->rx_sw_drop_bytes +
			sw_stats_prev->rx_sw_drop_bytes;

	if (adapter->dev_type != SXE2_DEV_T_VF) {
		stats->rx_out_of_buffer = hw_stats->rx_out_of_buffer;
		stats->rx_qblock_drop = hw_stats->rx_qblock_drop;
		stats->tx_frame_good = hw_stats->tx_frame_good;
		stats->rx_frame_good = hw_stats->rx_frame_good;
		stats->rx_crc_errors = hw_stats->rx_crc_errors;
		stats->tx_bytes_good = hw_stats->tx_bytes_good;
		stats->rx_bytes_good = hw_stats->rx_bytes_good;
		stats->tx_multicast_good = hw_stats->tx_multicast_good;
		stats->tx_broadcast_good = hw_stats->tx_broadcast_good;
		stats->rx_multicast_good = hw_stats->rx_multicast_good;
		stats->rx_broadcast_good = hw_stats->rx_broadcast_good;
		stats->rx_len_errors = hw_stats->rx_len_errors;
		stats->rx_out_of_range_errors = hw_stats->rx_out_of_range_errors;
		stats->rx_oversize_pkts_phy = hw_stats->rx_oversize_pkts_phy;
		stats->rx_symbol_err = hw_stats->rx_symbol_err;
		stats->rx_pause_frame = hw_stats->rx_pause_frame;
		stats->tx_pause_frame = hw_stats->tx_pause_frame;
		stats->rx_discards_phy = hw_stats->rx_discards_phy;
		stats->rx_discards_ips_phy = hw_stats->rx_discards_ips_phy;
		stats->tx_dropped_link_down = hw_stats->tx_dropped_link_down;
		stats->rx_undersize_good = hw_stats->rx_undersize_good;
		stats->rx_runt_error = hw_stats->rx_runt_error;
		stats->tx_bytes_good_bad = hw_stats->tx_bytes_good_bad;
		stats->tx_frame_good_bad = hw_stats->tx_frame_good_bad;
		stats->rx_jabbers = hw_stats->rx_jabbers;
		stats->rx_size_64 = hw_stats->rx_size_64;
		stats->rx_size_65_127 = hw_stats->rx_size_65_127;
		stats->rx_size_128_255 = hw_stats->rx_size_128_255;
		stats->rx_size_256_511 = hw_stats->rx_size_256_511;
		stats->rx_size_512_1023 = hw_stats->rx_size_512_1023;
		stats->rx_size_1024_1522 = hw_stats->rx_size_1024_1522;
		stats->rx_size_1523_max = hw_stats->rx_size_1523_max;
		stats->rx_pcs_symbol_err_phy = hw_stats->rx_pcs_symbol_err_phy;
		stats->rx_corrected_bits_phy = hw_stats->rx_corrected_bits_phy;
		stats->rx_err_lane_0_phy = hw_stats->rx_err_lane_0_phy;
		stats->rx_err_lane_1_phy = hw_stats->rx_err_lane_1_phy;
		stats->rx_err_lane_2_phy = hw_stats->rx_err_lane_2_phy;
		stats->rx_err_lane_3_phy = hw_stats->rx_err_lane_3_phy;
		stats->rx_illegal_bytes = hw_stats->rx_illegal_bytes;
		stats->rx_oversize_good = hw_stats->rx_oversize_good;
		stats->tx_unicast = hw_stats->tx_unicast;
		stats->tx_broadcast = hw_stats->tx_broadcast;
		stats->tx_multicast = hw_stats->tx_multicast;
		stats->tx_vlan_packet_good = hw_stats->tx_vlan_packet_good;
		stats->tx_size_64 = hw_stats->tx_size_64;
		stats->tx_size_65_127 = hw_stats->tx_size_65_127;
		stats->tx_size_128_255 = hw_stats->tx_size_128_255;
		stats->tx_size_256_511 = hw_stats->tx_size_256_511;
		stats->tx_size_512_1023 = hw_stats->tx_size_512_1023;
		stats->tx_size_1024_1522 = hw_stats->tx_size_1024_1522;
		stats->tx_size_1523_max = hw_stats->tx_size_1523_max;
		stats->tx_underflow_error = hw_stats->tx_underflow_error;
		stats->rx_byte_good_bad = hw_stats->rx_byte_good_bad;
		stats->rx_frame_good_bad = hw_stats->rx_frame_good_bad;
		stats->rx_unicast_good = hw_stats->rx_unicast_good;
		stats->rx_vlan_packets = hw_stats->rx_vlan_packets;
		rte_memcpy(stats->rx_prio_buf_discard, hw_stats->rx_prio_buf_discard,
				sizeof(hw_stats->rx_prio_buf_discard));
		rte_memcpy(stats->prio_xoff_rx, hw_stats->prio_xoff_rx,
				sizeof(hw_stats->prio_xoff_rx));
		rte_memcpy(stats->prio_xon_rx, hw_stats->prio_xon_rx,
				sizeof(hw_stats->prio_xon_rx));
		rte_memcpy(stats->prio_xon_tx, hw_stats->prio_xon_tx,
				sizeof(hw_stats->prio_xon_tx));
		rte_memcpy(stats->prio_xoff_tx, hw_stats->prio_xoff_tx,
				sizeof(hw_stats->prio_xoff_tx));
		rte_memcpy(stats->prio_xon_2_xoff, hw_stats->prio_xon_2_xoff,
				sizeof(hw_stats->prio_xon_2_xoff));

		stats->imissed = hw_stats->rx_out_of_buffer +
				hw_stats->rx_qblock_drop;
		for (i = 0; i < SXE2_MAX_USER_PRIORITY; i++)
			stats->imissed += hw_stats->rx_prio_buf_discard[i];
	}
}

int32_t sxe2_stats_info_get(struct rte_eth_dev *dev,
			struct rte_eth_stats *stats,
			struct eth_queue_stats *qstats)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter  = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi     *vsi      = adapter->vsi_ctxt.main_vsi;
	struct sxe2_stats   *stats_out = &vsi->vsi_stats.stats;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return sxe2_mp_req_get_stats(dev, stats, qstats);

	ret = sxe2_vsi_hw_stats_get_update(adapter);
	if (ret)
		goto end;

	ret = sxe2_vsi_sw_stats_get_update(adapter);
	if (ret)
		goto end;

	ret = sxe2_drv_queue_info_get_update(adapter, qstats);
	if (ret)
		goto end;

	sxe2_stats_update(adapter);

	stats->ipackets = stats_out->ipackets;
	stats->ibytes   = stats_out->ibytes;
	stats->ierrors  = stats_out->ierrors;
	stats->imissed  = stats_out->imissed;
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;

	stats->opackets = stats_out->opackets;
	stats->obytes   = stats_out->obytes;

	ret = 0;

end:
	return ret;
}

int32_t sxe2_xstats_info_get(struct rte_eth_dev *dev,
		struct rte_eth_xstat *xstats, uint32_t usr_cnt)
{
	uint32_t i      = 0;
	uint32_t cnt    = 0;
	int32_t ret    = 0;
	uint32_t offset = 0;
	uint32_t xstats_cnt = 0;
	struct sxe2_adapter    *adapter   = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi        *vsi       = adapter->vsi_ctxt.main_vsi;
	struct sxe2_vsi_stats  *xstats_out = &vsi->vsi_stats;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return sxe2_mp_req_get_xstats(dev, xstats, usr_cnt);

	if (adapter->dev_type == SXE2_DEV_T_VF)
		xstats_cnt = SXE2_XSTAT_CNT_VF;
	else
		xstats_cnt = SXE2_XSTAT_CNT_PF;

	if (usr_cnt < xstats_cnt) {
		ret = xstats_cnt;
		PMD_LOG_ERR(DRV, "user usr_cnt:%u less than stats cnt:%u", usr_cnt, ret);
		goto end;
	}

	ret = sxe2_vsi_hw_stats_get_update(adapter);
	if (ret) {
		ret = xstats_cnt;
		goto end;
	}

	ret = sxe2_vsi_sw_stats_get_update(adapter);
	if (ret) {
		ret = xstats_cnt;
		goto end;
	}

	if (adapter->dev_type == SXE2_DEV_T_VF) {
		sxe2_stats_update(adapter);
		for (i = 0; i < xstats_cnt; i++) {
			(void)sxe2_xstat_vf_offset_get(i, &offset);
			xstats[cnt].value = *(uint64_t *)(((int8_t *)(&xstats_out->stats)) +
							  offset);
			xstats[cnt].id    = cnt;
			cnt++;
		}
	} else {
		ret = sxe2_mac_hw_stats_get_update(adapter);
		if (ret) {
			ret = xstats_cnt;
			goto end;
		}

		sxe2_stats_update(adapter);

		for (i = 0; i < xstats_cnt; i++) {
			(void)sxe2_xstat_pf_offset_get(i, &offset);
			xstats[cnt].value = *(uint64_t *)(((int8_t *)(&xstats_out->stats)) +
							  offset);
			xstats[cnt].id = cnt;
			cnt++;
		}
	}
	ret = cnt;
	PMD_LOG_DEBUG(DRV, "usr_cnt:%u stats cnt:%u stats done", usr_cnt, cnt);

end:
	return ret;
}

int32_t sxe2_xstats_names_get(__rte_unused struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names,
	__rte_unused unsigned int usr_cnt)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_stats_field *field = NULL;
	uint32_t i   = 0;
	uint32_t cnt = 0;
	int32_t ret = -1;
	uint32_t xstats_cnt = 0;

	if (adapter->dev_type == SXE2_DEV_T_VF) {
		field = sxe2_xstats_field_vf;
		xstats_cnt = SXE2_XSTAT_CNT_VF;
	} else {
		field = sxe2_xstats_field_pf;
		xstats_cnt = SXE2_XSTAT_CNT_PF;
	}

	if (!xstats_names) {
		ret = xstats_cnt;
		PMD_LOG_DEBUG(DRV, "xstats field size:%u", ret);
		goto l_out;
	}

	if (usr_cnt < xstats_cnt) {
		ret = -EINVAL;
		PMD_LOG_ERR(DRV, "max:%d usr_cnt:%u invalid (err:%d)", xstats_cnt, usr_cnt, ret);
		goto l_out;
	}

	for (i = 0; i < xstats_cnt; i++) {
		(void)strlcpy(xstats_names[cnt].name, field[i].name,
			      sizeof(xstats_names[cnt].name));
		cnt++;
	}

	ret = cnt;

l_out:
	return ret;
}

int32_t sxe2_stats_hw_reset(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter    *adapter   = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi        *vsi       = adapter->vsi_ctxt.main_vsi;
	struct sxe2_rx_queue   *rxq;
	uint32_t rx_queue_id;

	ret = sxe2_drv_vsi_stats_reset(adapter);
	if (ret) {
		PMD_LOG_ERR(DRV, "reset vsi stats failed, ret:%d.", ret);
		goto l_end;
	}
	if (adapter->dev_type != SXE2_DEV_T_VF) {
		ret = sxe2_drv_mac_stats_reset(adapter);
		if (ret) {
			PMD_LOG_ERR(DRV, "reset mac stats failed, ret:%d.", ret);
			goto l_end;
		}
	}

	memset(&vsi->vsi_stats, 0, sizeof(vsi->vsi_stats));
	for (rx_queue_id = 0; rx_queue_id < dev->data->nb_rx_queues; rx_queue_id++) {
		rxq = dev->data->rx_queues[rx_queue_id];
		if (rxq)
			memset(&rxq->sw_stats, 0, sizeof(rxq->sw_stats));
	}

l_end:
	return ret;
}

int32_t sxe2_stats_info_reset(struct rte_eth_dev *dev)
{
	int32_t ret;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return sxe2_mp_req_reset_stats(dev);

	if (adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_Q_MAP) {
		ret = sxe2_drv_mapping_stats_info_clear(dev);
		if (ret)
			goto l_end;
	}

	ret = sxe2_stats_hw_reset(dev);
	if (ret)
		goto l_end;

l_end:
	return ret;
}

int32_t sxe2_stats_init(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	int32_t ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = sxe2_queue_stats_map_init(dev);
	if (ret)
		goto l_end;

	ret = sxe2_stats_hw_reset(dev);
	if (ret)
		goto l_end;

l_end:
	return ret;
}

int32_t sxe2_queue_stats_mapping_set(struct rte_eth_dev *eth_dev,
				  uint16_t queue_id, uint8_t pool_idx, uint8_t is_rx)
{
	int32_t ret = -1;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);

	if (!(adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_Q_MAP)) {
		PMD_LOG_ERR(DRV, "VF does not support queue mapping! ");
		goto l_end;
	}

	if (is_rx)
		ret = sxe2_drv_rxq_mapping_set(eth_dev, queue_id, pool_idx);
	else
		ret = sxe2_drv_txq_mapping_set(eth_dev, queue_id, pool_idx);

	if (ret) {
		PMD_LOG_ERR(DRV, "Queue stats mapping failed ! "
			"queue_id:%u pool_idx:%u", queue_id, pool_idx);
		goto l_end;
	}

	PMD_LOG_DEBUG(DRV, "port %u %s queue_id %d stat map to pool[%u] ",
		     (uint16_t)(eth_dev->data->port_id), is_rx ? "RX" : "TX",
		     queue_id, pool_idx);
l_end:
	return ret;
}

int32_t sxe2_queue_stats_map_init(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_Q_MAP) {
		dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

		ret = sxe2_drv_mapping_reset(dev);
		if (ret) {
			PMD_LOG_ERR(DRV, "Queue stats mapping init failed !");
			goto l_end;
		}
	}

l_end:
	return ret;
}
