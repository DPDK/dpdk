/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <ethdev_pci.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_ether.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_hwif.h"
#include "base/hinic_pmd_wq.h"
#include "base/hinic_pmd_cfg.h"
#include "base/hinic_pmd_mgmt.h"
#include "base/hinic_pmd_cmdq.h"
#include "base/hinic_pmd_niccfg.h"
#include "base/hinic_pmd_nicio.h"
#include "base/hinic_pmd_mbox.h"
#include "hinic_pmd_ethdev.h"
#include "hinic_pmd_tx.h"
#include "hinic_pmd_rx.h"

/* Vendor ID used by Huawei devices */
#define HINIC_HUAWEI_VENDOR_ID		0x19E5

/* Hinic devices */
#define HINIC_DEV_ID_PRD		0x1822
#define HINIC_DEV_ID_VF			0x375E
#define HINIC_DEV_ID_VF_HV		0x379E

/* Mezz card for Blade Server */
#define HINIC_DEV_ID_MEZZ_25GE		0x0210
#define HINIC_DEV_ID_MEZZ_100GE		0x0205

/* 2*25G and 2*100G card */
#define HINIC_DEV_ID_1822_DUAL_25GE	0x0206
#define HINIC_DEV_ID_1822_100GE		0x0200

#define HINIC_SERVICE_MODE_NIC		2

#define HINIC_INTR_CB_UNREG_MAX_RETRIES	10

#define DEFAULT_BASE_COS		4
#define NR_MAX_COS			8

#define HINIC_MIN_RX_BUF_SIZE		1024
#define HINIC_MAX_UC_MAC_ADDRS		128
#define HINIC_MAX_MC_MAC_ADDRS		2048

#define HINIC_DEFAULT_BURST_SIZE	32
#define HINIC_DEFAULT_NB_QUEUES		1
#define HINIC_DEFAULT_RING_SIZE		1024
#define HINIC_MAX_LRO_SIZE		65536

/*
 * vlan_id is a 12 bit number.
 * The VFTA array is actually a 4096 bit array, 128 of 32bit elements.
 * 2^5 = 32. The val of lower 5 bits specifies the bit in the 32bit element.
 * The higher 7 bit val specifies VFTA array index.
 */
#define HINIC_VFTA_BIT(vlan_id)    (1 << ((vlan_id) & 0x1F))
#define HINIC_VFTA_IDX(vlan_id)    ((vlan_id) >> 5)

#define HINIC_VLAN_FILTER_EN		(1U << 0)

/* lro numer limit for one packet */
#define HINIC_LRO_WQE_NUM_DEFAULT	8

struct hinic_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	u32  offset;
};

#define HINIC_FUNC_STAT(_stat_item) {	\
	.name = #_stat_item, \
	.offset = offsetof(struct hinic_vport_stats, _stat_item) \
}

#define HINIC_PORT_STAT(_stat_item) { \
	.name = #_stat_item, \
	.offset = offsetof(struct hinic_phy_port_stats, _stat_item) \
}

static const struct hinic_xstats_name_off hinic_vport_stats_strings[] = {
	HINIC_FUNC_STAT(tx_unicast_pkts_vport),
	HINIC_FUNC_STAT(tx_unicast_bytes_vport),
	HINIC_FUNC_STAT(tx_multicast_pkts_vport),
	HINIC_FUNC_STAT(tx_multicast_bytes_vport),
	HINIC_FUNC_STAT(tx_broadcast_pkts_vport),
	HINIC_FUNC_STAT(tx_broadcast_bytes_vport),

	HINIC_FUNC_STAT(rx_unicast_pkts_vport),
	HINIC_FUNC_STAT(rx_unicast_bytes_vport),
	HINIC_FUNC_STAT(rx_multicast_pkts_vport),
	HINIC_FUNC_STAT(rx_multicast_bytes_vport),
	HINIC_FUNC_STAT(rx_broadcast_pkts_vport),
	HINIC_FUNC_STAT(rx_broadcast_bytes_vport),

	HINIC_FUNC_STAT(tx_discard_vport),
	HINIC_FUNC_STAT(rx_discard_vport),
	HINIC_FUNC_STAT(tx_err_vport),
	HINIC_FUNC_STAT(rx_err_vport),
};

#define HINIC_VPORT_XSTATS_NUM (sizeof(hinic_vport_stats_strings) / \
		sizeof(hinic_vport_stats_strings[0]))

static const struct hinic_xstats_name_off hinic_phyport_stats_strings[] = {
	HINIC_PORT_STAT(mac_rx_total_pkt_num),
	HINIC_PORT_STAT(mac_rx_total_oct_num),
	HINIC_PORT_STAT(mac_rx_bad_pkt_num),
	HINIC_PORT_STAT(mac_rx_bad_oct_num),
	HINIC_PORT_STAT(mac_rx_good_pkt_num),
	HINIC_PORT_STAT(mac_rx_good_oct_num),
	HINIC_PORT_STAT(mac_rx_uni_pkt_num),
	HINIC_PORT_STAT(mac_rx_multi_pkt_num),
	HINIC_PORT_STAT(mac_rx_broad_pkt_num),
	HINIC_PORT_STAT(mac_tx_total_pkt_num),
	HINIC_PORT_STAT(mac_tx_total_oct_num),
	HINIC_PORT_STAT(mac_tx_bad_pkt_num),
	HINIC_PORT_STAT(mac_tx_bad_oct_num),
	HINIC_PORT_STAT(mac_tx_good_pkt_num),
	HINIC_PORT_STAT(mac_tx_good_oct_num),
	HINIC_PORT_STAT(mac_tx_uni_pkt_num),
	HINIC_PORT_STAT(mac_tx_multi_pkt_num),
	HINIC_PORT_STAT(mac_tx_broad_pkt_num),
	HINIC_PORT_STAT(mac_rx_fragment_pkt_num),
	HINIC_PORT_STAT(mac_rx_undersize_pkt_num),
	HINIC_PORT_STAT(mac_rx_undermin_pkt_num),
	HINIC_PORT_STAT(mac_rx_64_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_65_127_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_128_255_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_256_511_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_512_1023_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_1024_1518_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_1519_2047_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_2048_4095_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_4096_8191_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_8192_9216_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_9217_12287_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_12288_16383_oct_pkt_num),
	HINIC_PORT_STAT(mac_rx_1519_max_bad_pkt_num),
	HINIC_PORT_STAT(mac_rx_1519_max_good_pkt_num),
	HINIC_PORT_STAT(mac_rx_oversize_pkt_num),
	HINIC_PORT_STAT(mac_rx_jabber_pkt_num),
	HINIC_PORT_STAT(mac_rx_mac_pause_num),
	HINIC_PORT_STAT(mac_rx_pfc_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri0_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri1_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri2_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri3_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri4_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri5_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri6_pkt_num),
	HINIC_PORT_STAT(mac_rx_pfc_pri7_pkt_num),
	HINIC_PORT_STAT(mac_rx_mac_control_pkt_num),
	HINIC_PORT_STAT(mac_rx_sym_err_pkt_num),
	HINIC_PORT_STAT(mac_rx_fcs_err_pkt_num),
	HINIC_PORT_STAT(mac_rx_send_app_good_pkt_num),
	HINIC_PORT_STAT(mac_rx_send_app_bad_pkt_num),
	HINIC_PORT_STAT(mac_tx_fragment_pkt_num),
	HINIC_PORT_STAT(mac_tx_undersize_pkt_num),
	HINIC_PORT_STAT(mac_tx_undermin_pkt_num),
	HINIC_PORT_STAT(mac_tx_64_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_65_127_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_128_255_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_256_511_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_512_1023_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_1024_1518_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_1519_2047_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_2048_4095_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_4096_8191_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_8192_9216_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_9217_12287_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_12288_16383_oct_pkt_num),
	HINIC_PORT_STAT(mac_tx_1519_max_bad_pkt_num),
	HINIC_PORT_STAT(mac_tx_1519_max_good_pkt_num),
	HINIC_PORT_STAT(mac_tx_oversize_pkt_num),
	HINIC_PORT_STAT(mac_trans_jabber_pkt_num),
	HINIC_PORT_STAT(mac_tx_mac_pause_num),
	HINIC_PORT_STAT(mac_tx_pfc_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri0_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri1_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri2_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri3_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri4_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri5_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri6_pkt_num),
	HINIC_PORT_STAT(mac_tx_pfc_pri7_pkt_num),
	HINIC_PORT_STAT(mac_tx_mac_control_pkt_num),
	HINIC_PORT_STAT(mac_tx_err_all_pkt_num),
	HINIC_PORT_STAT(mac_tx_from_app_good_pkt_num),
	HINIC_PORT_STAT(mac_tx_from_app_bad_pkt_num),
};

#define HINIC_PHYPORT_XSTATS_NUM (sizeof(hinic_phyport_stats_strings) / \
		sizeof(hinic_phyport_stats_strings[0]))

static const struct hinic_xstats_name_off hinic_rxq_stats_strings[] = {
	{"rx_nombuf", offsetof(struct hinic_rxq_stats, rx_nombuf)},
	{"burst_pkt", offsetof(struct hinic_rxq_stats, burst_pkts)},
};

#define HINIC_RXQ_XSTATS_NUM (sizeof(hinic_rxq_stats_strings) / \
		sizeof(hinic_rxq_stats_strings[0]))

static const struct hinic_xstats_name_off hinic_txq_stats_strings[] = {
	{"tx_busy", offsetof(struct hinic_txq_stats, tx_busy)},
	{"offload_errors", offsetof(struct hinic_txq_stats, off_errs)},
	{"copy_pkts", offsetof(struct hinic_txq_stats, cpy_pkts)},
	{"rl_drop", offsetof(struct hinic_txq_stats, rl_drop)},
	{"burst_pkts", offsetof(struct hinic_txq_stats, burst_pkts)},
	{"sge_len0", offsetof(struct hinic_txq_stats, sge_len0)},
	{"mbuf_null", offsetof(struct hinic_txq_stats, mbuf_null)},
};

#define HINIC_TXQ_XSTATS_NUM (sizeof(hinic_txq_stats_strings) / \
		sizeof(hinic_txq_stats_strings[0]))

static int hinic_xstats_calc_num(struct hinic_nic_dev *nic_dev)
{
	if (HINIC_IS_VF(nic_dev->hwdev)) {
		return (HINIC_VPORT_XSTATS_NUM +
			HINIC_RXQ_XSTATS_NUM * nic_dev->num_rq +
			HINIC_TXQ_XSTATS_NUM * nic_dev->num_sq);
	} else {
		return (HINIC_VPORT_XSTATS_NUM +
			HINIC_PHYPORT_XSTATS_NUM +
			HINIC_RXQ_XSTATS_NUM * nic_dev->num_rq +
			HINIC_TXQ_XSTATS_NUM * nic_dev->num_sq);
	}
}

static const struct rte_eth_desc_lim hinic_rx_desc_lim = {
	.nb_max = HINIC_MAX_QUEUE_DEPTH,
	.nb_min = HINIC_MIN_QUEUE_DEPTH,
	.nb_align = HINIC_RXD_ALIGN,
};

static const struct rte_eth_desc_lim hinic_tx_desc_lim = {
	.nb_max = HINIC_MAX_QUEUE_DEPTH,
	.nb_min = HINIC_MIN_QUEUE_DEPTH,
	.nb_align = HINIC_TXD_ALIGN,
};

static int hinic_vlan_offload_set(struct rte_eth_dev *dev, int mask);

/**
 * Interrupt handler triggered by NIC  for handling
 * specific event.
 *
 * @param: The address of parameter (struct rte_eth_dev *) registered before.
 */
static void hinic_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (!rte_bit_relaxed_get32(HINIC_DEV_INTR_EN, &nic_dev->dev_status)) {
		PMD_DRV_LOG(WARNING, "Device's interrupt is disabled, ignore interrupt event, dev_name: %s, port_id: %d",
			    nic_dev->proc_dev_name, dev->data->port_id);
		return;
	}

	/* aeq0 msg handler */
	hinic_dev_handle_aeq_event(nic_dev->hwdev, param);
}

/**
 * Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues, mtu size
 * and configure RSS.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_dev_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev;
	struct hinic_nic_io *nic_io;
	int err;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	nic_io = nic_dev->hwdev->nic_io;

	nic_dev->num_sq =  dev->data->nb_tx_queues;
	nic_dev->num_rq = dev->data->nb_rx_queues;

	nic_io->num_sqs =  dev->data->nb_tx_queues;
	nic_io->num_rqs = dev->data->nb_rx_queues;

	/* queue pair is max_num(sq, rq) */
	nic_dev->num_qps = (nic_dev->num_sq > nic_dev->num_rq) ?
			nic_dev->num_sq : nic_dev->num_rq;
	nic_io->num_qps = nic_dev->num_qps;

	if (nic_dev->num_qps > nic_io->max_qps) {
		PMD_DRV_LOG(ERR,
			"Queue number out of range, get queue_num:%d, max_queue_num:%d",
			nic_dev->num_qps, nic_io->max_qps);
		return -EINVAL;
	}

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* mtu size is 256~9600 */
	if (HINIC_MTU_TO_PKTLEN(dev->data->dev_conf.rxmode.mtu) <
			HINIC_MIN_FRAME_SIZE ||
	    HINIC_MTU_TO_PKTLEN(dev->data->dev_conf.rxmode.mtu) >
			HINIC_MAX_JUMBO_FRAME_SIZE) {
		PMD_DRV_LOG(ERR,
			"Packet length out of range, get packet length:%d, "
			"expect between %d and %d",
			HINIC_MTU_TO_PKTLEN(dev->data->dev_conf.rxmode.mtu),
			HINIC_MIN_FRAME_SIZE, HINIC_MAX_JUMBO_FRAME_SIZE);
		return -EINVAL;
	}

	nic_dev->mtu_size = dev->data->dev_conf.rxmode.mtu;

	/* rss template */
	err = hinic_config_mq_mode(dev, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Config multi-queue failed");
		return err;
	}

	/* init VLAN offload */
	err = hinic_vlan_offload_set(dev,
				RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize vlan filter and strip failed");
		(void)hinic_config_mq_mode(dev, FALSE);
		return err;
	}

	/* clear fdir filter flag in function table */
	hinic_free_fdir_filter(nic_dev);

	return HINIC_OK;
}

/**
 * DPDK callback to create the receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param queue_idx
 *   RX queue index.
 * @param nb_desc
 *   Number of descriptors for receive queue.
 * @param socket_id
 *   NUMA socket on which memory must be allocated.
 * @param rx_conf
 *   Thresholds parameters (unused_).
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			 uint16_t nb_desc, unsigned int socket_id,
			 __rte_unused const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	int rc;
	struct hinic_nic_dev *nic_dev;
	struct hinic_hwdev *hwdev;
	struct hinic_rxq *rxq;
	u16 rq_depth, rx_free_thresh;
	u32 buf_size;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hwdev = nic_dev->hwdev;

	/* queue depth must be power of 2, otherwise will be aligned up */
	rq_depth = (nb_desc & (nb_desc - 1)) ?
		((u16)(1U << (ilog2(nb_desc) + 1))) : nb_desc;

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum and minimum.
	 */
	if (rq_depth > HINIC_MAX_QUEUE_DEPTH ||
		rq_depth < HINIC_MIN_QUEUE_DEPTH) {
		PMD_DRV_LOG(ERR, "RX queue depth is out of range from %d to %d, (nb_desc=%d, q_depth=%d, port=%d queue=%d)",
			    HINIC_MIN_QUEUE_DEPTH, HINIC_MAX_QUEUE_DEPTH,
			    (int)nb_desc, (int)rq_depth,
			    (int)dev->data->port_id, (int)queue_idx);
		return -EINVAL;
	}

	/*
	 * The RX descriptor ring will be cleaned after rxq->rx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free RX
	 * descriptors.
	 * The following constraints must be satisfied:
	 *  rx_free_thresh must be greater than 0.
	 *  rx_free_thresh must be less than the size of the ring minus 1.
	 * When set to zero use default values.
	 */
	rx_free_thresh = (u16)((rx_conf->rx_free_thresh) ?
			rx_conf->rx_free_thresh : HINIC_DEFAULT_RX_FREE_THRESH);
	if (rx_free_thresh >= (rq_depth - 1)) {
		PMD_DRV_LOG(ERR, "rx_free_thresh must be less than the number of RX descriptors minus 1. (rx_free_thresh=%u port=%d queue=%d)",
			    (unsigned int)rx_free_thresh,
			    (int)dev->data->port_id,
			    (int)queue_idx);
		return -EINVAL;
	}

	rxq = rte_zmalloc_socket("hinic_rx_queue", sizeof(struct hinic_rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Allocate rxq[%d] failed, dev_name: %s",
			    queue_idx, dev->data->name);
		return -ENOMEM;
	}
	nic_dev->rxqs[queue_idx] = rxq;

	/* alloc rx sq hw wqe page */
	rc = hinic_create_rq(hwdev, queue_idx, rq_depth, socket_id);
	if (rc) {
		PMD_DRV_LOG(ERR, "Create rxq[%d] failed, dev_name: %s, rq_depth: %d",
			    queue_idx, dev->data->name, rq_depth);
		goto ceate_rq_fail;
	}

	/* mbuf pool must be assigned before setup rx resources */
	rxq->mb_pool = mp;

	rc =
	hinic_convert_rx_buf_size(rte_pktmbuf_data_room_size(rxq->mb_pool) -
				  RTE_PKTMBUF_HEADROOM, &buf_size);
	if (rc) {
		PMD_DRV_LOG(ERR, "Adjust buf size failed, dev_name: %s",
			    dev->data->name);
		goto adjust_bufsize_fail;
	}

	/* rx queue info, rearm control */
	rxq->wq = &hwdev->nic_io->rq_wq[queue_idx];
	rxq->pi_virt_addr = hwdev->nic_io->qps[queue_idx].rq.pi_virt_addr;
	rxq->nic_dev = nic_dev;
	rxq->q_id = queue_idx;
	rxq->q_depth = rq_depth;
	rxq->buf_len = (u16)buf_size;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->socket_id = socket_id;

	/* the last point cant do mbuf rearm in bulk */
	rxq->rxinfo_align_end = rxq->q_depth - rxq->rx_free_thresh;

	/* device port identifier */
	rxq->port_id = dev->data->port_id;

	/* alloc rx_cqe and prepare rq_wqe */
	rc = hinic_setup_rx_resources(rxq);
	if (rc) {
		PMD_DRV_LOG(ERR, "Setup rxq[%d] rx_resources failed, dev_name: %s",
			    queue_idx, dev->data->name);
		goto setup_rx_res_err;
	}

	/* record nic_dev rxq in rte_eth rx_queues */
	dev->data->rx_queues[queue_idx] = rxq;

	return 0;

setup_rx_res_err:
adjust_bufsize_fail:
	hinic_destroy_rq(hwdev, queue_idx);

ceate_rq_fail:
	rte_free(rxq);

	return rc;
}

static void hinic_reset_rx_queue(struct rte_eth_dev *dev)
{
	struct hinic_rxq *rxq;
	struct hinic_nic_dev *nic_dev;
	int q_id = 0;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++) {
		rxq = dev->data->rx_queues[q_id];

		rxq->wq->cons_idx = 0;
		rxq->wq->prod_idx = 0;
		rxq->wq->delta = rxq->q_depth;
		rxq->wq->mask = rxq->q_depth - 1;

		/* alloc mbuf to rq */
		hinic_rx_alloc_pkts(rxq);
	}
}

/**
 * DPDK callback to configure the transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param queue_idx
 *   Transmit queue index.
 * @param nb_desc
 *   Number of descriptors for transmit queue.
 * @param socket_id
 *   NUMA socket on which memory must be allocated.
 * @param tx_conf
 *   Tx queue configuration parameters.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			 uint16_t nb_desc, unsigned int socket_id,
			 __rte_unused const struct rte_eth_txconf *tx_conf)
{
	int rc;
	struct hinic_nic_dev *nic_dev;
	struct hinic_hwdev *hwdev;
	struct hinic_txq *txq;
	u16 sq_depth, tx_free_thresh;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hwdev = nic_dev->hwdev;

	/* queue depth must be power of 2, otherwise will be aligned up */
	sq_depth = (nb_desc & (nb_desc - 1)) ?
			((u16)(1U << (ilog2(nb_desc) + 1))) : nb_desc;

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum and minimum.
	 */
	if (sq_depth > HINIC_MAX_QUEUE_DEPTH ||
		sq_depth < HINIC_MIN_QUEUE_DEPTH) {
		PMD_DRV_LOG(ERR, "TX queue depth is out of range from %d to %d, (nb_desc=%d, q_depth=%d, port=%d queue=%d)",
			  HINIC_MIN_QUEUE_DEPTH, HINIC_MAX_QUEUE_DEPTH,
			  (int)nb_desc, (int)sq_depth,
			  (int)dev->data->port_id, (int)queue_idx);
		return -EINVAL;
	}

	/*
	 * The TX descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free TX
	 * descriptors.
	 * The following constraints must be satisfied:
	 *  tx_free_thresh must be greater than 0.
	 *  tx_free_thresh must be less than the size of the ring minus 1.
	 * When set to zero use default values.
	 */
	tx_free_thresh = (u16)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : HINIC_DEFAULT_TX_FREE_THRESH);
	if (tx_free_thresh >= (sq_depth - 1)) {
		PMD_DRV_LOG(ERR, "tx_free_thresh must be less than the number of TX descriptors minus 1. (tx_free_thresh=%u port=%d queue=%d)",
			(unsigned int)tx_free_thresh, (int)dev->data->port_id,
			(int)queue_idx);
		return -EINVAL;
	}

	txq = rte_zmalloc_socket("hinic_tx_queue", sizeof(struct hinic_txq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Allocate txq[%d] failed, dev_name: %s",
			    queue_idx, dev->data->name);
		return -ENOMEM;
	}
	nic_dev->txqs[queue_idx] = txq;

	/* alloc tx sq hw wqepage */
	rc = hinic_create_sq(hwdev, queue_idx, sq_depth, socket_id);
	if (rc) {
		PMD_DRV_LOG(ERR, "Create txq[%d] failed, dev_name: %s, sq_depth: %d",
			    queue_idx, dev->data->name, sq_depth);
		goto create_sq_fail;
	}

	txq->q_id = queue_idx;
	txq->q_depth = sq_depth;
	txq->port_id = dev->data->port_id;
	txq->tx_free_thresh = tx_free_thresh;
	txq->nic_dev = nic_dev;
	txq->wq = &hwdev->nic_io->sq_wq[queue_idx];
	txq->sq = &hwdev->nic_io->qps[queue_idx].sq;
	txq->cons_idx_addr = hwdev->nic_io->qps[queue_idx].sq.cons_idx_addr;
	txq->sq_head_addr = HINIC_GET_WQ_HEAD(txq);
	txq->sq_bot_sge_addr = HINIC_GET_WQ_TAIL(txq) -
					sizeof(struct hinic_sq_bufdesc);
	txq->cos = nic_dev->default_cos;
	txq->socket_id = socket_id;

	/* alloc software txinfo */
	rc = hinic_setup_tx_resources(txq);
	if (rc) {
		PMD_DRV_LOG(ERR, "Setup txq[%d] tx_resources failed, dev_name: %s",
			    queue_idx, dev->data->name);
		goto setup_tx_res_fail;
	}

	/* record nic_dev txq in rte_eth tx_queues */
	dev->data->tx_queues[queue_idx] = txq;

	return HINIC_OK;

setup_tx_res_fail:
	hinic_destroy_sq(hwdev, queue_idx);

create_sq_fail:
	rte_free(txq);

	return rc;
}

static void hinic_reset_tx_queue(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev;
	struct hinic_txq *txq;
	struct hinic_nic_io *nic_io;
	struct hinic_hwdev *hwdev;
	volatile u32 *ci_addr;
	int q_id = 0;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hwdev = nic_dev->hwdev;
	nic_io = hwdev->nic_io;

	for (q_id = 0; q_id < nic_dev->num_sq; q_id++) {
		txq = dev->data->tx_queues[q_id];

		txq->wq->cons_idx = 0;
		txq->wq->prod_idx = 0;
		txq->wq->delta = txq->q_depth;
		txq->wq->mask  = txq->q_depth - 1;

		/* clear hardware ci */
		ci_addr = (volatile u32 *)HINIC_CI_VADDR(nic_io->ci_vaddr_base,
							q_id);
		*ci_addr = 0;
	}
}

/**
 * Get link speed from NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param speed_capa
 *   Pointer to link speed structure.
 */
static void hinic_get_speed_capa(struct rte_eth_dev *dev, uint32_t *speed_capa)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u32 supported_link, advertised_link;
	int err;

#define HINIC_LINK_MODE_SUPPORT_1G	(1U << HINIC_GE_BASE_KX)

#define HINIC_LINK_MODE_SUPPORT_10G	(1U << HINIC_10GE_BASE_KR)

#define HINIC_LINK_MODE_SUPPORT_25G	((1U << HINIC_25GE_BASE_KR_S) | \
					(1U << HINIC_25GE_BASE_CR_S) | \
					(1U << HINIC_25GE_BASE_KR) | \
					(1U << HINIC_25GE_BASE_CR))

#define HINIC_LINK_MODE_SUPPORT_40G	((1U << HINIC_40GE_BASE_KR4) | \
					(1U << HINIC_40GE_BASE_CR4))

#define HINIC_LINK_MODE_SUPPORT_100G	((1U << HINIC_100GE_BASE_KR4) | \
					(1U << HINIC_100GE_BASE_CR4))

	err = hinic_get_link_mode(nic_dev->hwdev,
				  &supported_link, &advertised_link);
	if (err || supported_link == HINIC_SUPPORTED_UNKNOWN ||
	    advertised_link == HINIC_SUPPORTED_UNKNOWN) {
		PMD_DRV_LOG(WARNING, "Get speed capability info failed, device: %s, port_id: %u",
			  nic_dev->proc_dev_name, dev->data->port_id);
	} else {
		*speed_capa = 0;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_1G))
			*speed_capa |= RTE_ETH_LINK_SPEED_1G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_10G))
			*speed_capa |= RTE_ETH_LINK_SPEED_10G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_25G))
			*speed_capa |= RTE_ETH_LINK_SPEED_25G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_40G))
			*speed_capa |= RTE_ETH_LINK_SPEED_40G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_100G))
			*speed_capa |= RTE_ETH_LINK_SPEED_100G;
	}
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param info
 *   Pointer to Info structure output buffer.
 */
static int
hinic_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	info->max_rx_queues  = nic_dev->nic_cap.max_rqs;
	info->max_tx_queues  = nic_dev->nic_cap.max_sqs;
	info->min_rx_bufsize = HINIC_MIN_RX_BUF_SIZE;
	info->max_rx_pktlen  = HINIC_MAX_JUMBO_FRAME_SIZE;
	info->max_mac_addrs  = HINIC_MAX_UC_MAC_ADDRS;
	info->min_mtu = HINIC_MIN_MTU_SIZE;
	info->max_mtu = HINIC_MAX_MTU_SIZE;
	info->max_lro_pkt_size = HINIC_MAX_LRO_SIZE;

	hinic_get_speed_capa(dev, &info->speed_capa);
	info->rx_queue_offload_capa = 0;
	info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
				RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
				RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
				RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
				RTE_ETH_RX_OFFLOAD_SCATTER |
				RTE_ETH_RX_OFFLOAD_TCP_LRO |
				RTE_ETH_RX_OFFLOAD_RSS_HASH;

	info->tx_queue_offload_capa = 0;
	info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
				RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
				RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
				RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
				RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				RTE_ETH_TX_OFFLOAD_TCP_TSO |
				RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;

	info->hash_key_size = HINIC_RSS_KEY_SIZE;
	info->reta_size = HINIC_RSS_INDIR_SIZE;
	info->flow_type_rss_offloads = HINIC_RSS_OFFLOAD_ALL;
	info->rx_desc_lim = hinic_rx_desc_lim;
	info->tx_desc_lim = hinic_tx_desc_lim;

	/* Driver-preferred Rx/Tx parameters */
	info->default_rxportconf.burst_size = HINIC_DEFAULT_BURST_SIZE;
	info->default_txportconf.burst_size = HINIC_DEFAULT_BURST_SIZE;
	info->default_rxportconf.nb_queues = HINIC_DEFAULT_NB_QUEUES;
	info->default_txportconf.nb_queues = HINIC_DEFAULT_NB_QUEUES;
	info->default_rxportconf.ring_size = HINIC_DEFAULT_RING_SIZE;
	info->default_txportconf.ring_size = HINIC_DEFAULT_RING_SIZE;

	return 0;
}

static int hinic_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
				size_t fw_size)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	char fw_ver[HINIC_MGMT_VERSION_MAX_LEN] = {0};
	int err;

	err = hinic_get_mgmt_version(nic_dev->hwdev, fw_ver);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get fw version");
		return -EINVAL;
	}

	if (fw_size < strlen(fw_ver) + 1)
		return (strlen(fw_ver) + 1);

	snprintf(fw_version, fw_size, "%s", fw_ver);

	return 0;
}

static int hinic_config_rx_mode(struct hinic_nic_dev *nic_dev, u32 rx_mode_ctrl)
{
	int err;

	err = hinic_set_rx_mode(nic_dev->hwdev, rx_mode_ctrl);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to set rx mode");
		return -EINVAL;
	}
	nic_dev->rx_mode_status = rx_mode_ctrl;

	return 0;
}

static int hinic_rxtx_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err;

	/* rx configure, if rss enable, need to init default configuration */
	err = hinic_rx_configure(dev);
	if (err) {
		PMD_DRV_LOG(ERR, "Configure rss failed");
		return err;
	}

	/* rx mode init */
	err = hinic_config_rx_mode(nic_dev, HINIC_DEFAULT_RX_MODE);
	if (err) {
		PMD_DRV_LOG(ERR, "Configure rx_mode:0x%x failed",
			HINIC_DEFAULT_RX_MODE);
		goto set_rx_mode_fail;
	}

	return HINIC_OK;

set_rx_mode_fail:
	hinic_rx_remove_configure(dev);

	return err;
}

static void hinic_remove_rxtx_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	(void)hinic_config_rx_mode(nic_dev, 0);
	hinic_rx_remove_configure(dev);
}

static int hinic_priv_get_dev_link_status(struct hinic_nic_dev *nic_dev,
					  struct rte_eth_link *link)
{
	int rc;
	u8 port_link_status = 0;
	struct nic_port_info port_link_info;
	struct hinic_hwdev *nic_hwdev = nic_dev->hwdev;
	uint32_t port_speed[LINK_SPEED_MAX] = {RTE_ETH_SPEED_NUM_10M,
					RTE_ETH_SPEED_NUM_100M, RTE_ETH_SPEED_NUM_1G,
					RTE_ETH_SPEED_NUM_10G, RTE_ETH_SPEED_NUM_25G,
					RTE_ETH_SPEED_NUM_40G, RTE_ETH_SPEED_NUM_100G};

	rc = hinic_get_link_status(nic_hwdev, &port_link_status);
	if (rc)
		return rc;

	if (!port_link_status) {
		link->link_status = RTE_ETH_LINK_DOWN;
		link->link_speed = 0;
		link->link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		link->link_autoneg = RTE_ETH_LINK_FIXED;
		return HINIC_OK;
	}

	memset(&port_link_info, 0, sizeof(port_link_info));
	rc = hinic_get_port_info(nic_hwdev, &port_link_info);
	if (rc)
		return rc;

	link->link_speed = port_speed[port_link_info.speed % LINK_SPEED_MAX];
	link->link_duplex = port_link_info.duplex;
	link->link_autoneg = port_link_info.autoneg_state;
	link->link_status = port_link_status;

	return HINIC_OK;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion.
 *
 * @return
 *   0 link status changed, -1 link status not changed
 */
static int hinic_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
#define CHECK_INTERVAL 10  /* 10ms */
#define MAX_REPEAT_TIME 100  /* 1s (100 * 10ms) in total */
	int rc = HINIC_OK;
	struct rte_eth_link link;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	unsigned int rep_cnt = MAX_REPEAT_TIME;

	memset(&link, 0, sizeof(link));
	do {
		/* Get link status information from hardware */
		rc = hinic_priv_get_dev_link_status(nic_dev, &link);
		if (rc != HINIC_OK) {
			link.link_speed = RTE_ETH_SPEED_NUM_NONE;
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			PMD_DRV_LOG(ERR, "Get link status failed");
			goto out;
		}

		if (!wait_to_complete || link.link_status)
			break;

		rte_delay_ms(CHECK_INTERVAL);
	} while (rep_cnt--);

out:
	rc = rte_eth_linkstatus_set(dev, &link);
	return rc;
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int hinic_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	/* link status follow phy port status, up will open pma */
	ret = hinic_set_port_enable(nic_dev->hwdev, true);
	if (ret)
		PMD_DRV_LOG(ERR, "Set mac link up failed, dev_name: %s, port_id: %d",
			    nic_dev->proc_dev_name, dev->data->port_id);

	return ret;
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int hinic_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	/* link status follow phy port status, up will close pma */
	ret = hinic_set_port_enable(nic_dev->hwdev, false);
	if (ret)
		PMD_DRV_LOG(ERR, "Set mac link down failed, dev_name: %s, port_id: %d",
			    nic_dev->proc_dev_name, dev->data->port_id);

	return ret;
}

/**
 * DPDK callback to start the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int hinic_dev_start(struct rte_eth_dev *dev)
{
	int rc;
	char *name;
	struct hinic_nic_dev *nic_dev;
	uint16_t i;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	name = dev->data->name;

	/* reset rx and tx queue */
	hinic_reset_rx_queue(dev);
	hinic_reset_tx_queue(dev);

	/* get func rx buf size */
	hinic_get_func_rx_buf_size(nic_dev);

	/* init txq and rxq context */
	rc = hinic_init_qp_ctxts(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize qp context failed, dev_name: %s",
			    name);
		goto init_qp_fail;
	}

	/* rss template */
	rc = hinic_config_mq_mode(dev, TRUE);
	if (rc) {
		PMD_DRV_LOG(ERR, "Configure mq mode failed, dev_name: %s",
			    name);
		goto cfg_mq_mode_fail;
	}

	/* set default mtu */
	rc = hinic_set_port_mtu(nic_dev->hwdev, nic_dev->mtu_size);
	if (rc) {
		PMD_DRV_LOG(ERR, "Set mtu_size[%d] failed, dev_name: %s",
			    nic_dev->mtu_size, name);
		goto set_mtu_fail;
	}

	/* configure rss rx_mode and other rx or tx default feature */
	rc = hinic_rxtx_configure(dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Configure tx and rx failed, dev_name: %s",
			    name);
		goto cfg_rxtx_fail;
	}

	/* reactive pf status, so that uP report asyn event */
	hinic_set_pf_status(nic_dev->hwdev->hwif, HINIC_PF_STATUS_ACTIVE_FLAG);

	/* open virtual port and ready to start packet receiving */
	rc = hinic_set_vport_enable(nic_dev->hwdev, true);
	if (rc) {
		PMD_DRV_LOG(ERR, "Enable vport failed, dev_name:%s", name);
		goto en_vport_fail;
	}

	/* open physical port and start packet receiving */
	rc = hinic_set_port_enable(nic_dev->hwdev, true);
	if (rc) {
		PMD_DRV_LOG(ERR, "Enable physical port failed, dev_name: %s",
			    name);
		goto en_port_fail;
	}

	/* update eth_dev link status */
	if (dev->data->dev_conf.intr_conf.lsc != 0)
		(void)hinic_link_update(dev, 0);

	rte_bit_relaxed_set32(HINIC_DEV_START, &nic_dev->dev_status);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;

en_port_fail:
	(void)hinic_set_vport_enable(nic_dev->hwdev, false);

en_vport_fail:
	hinic_set_pf_status(nic_dev->hwdev->hwif, HINIC_PF_STATUS_INIT);

	/* Flush tx && rx chip resources in case of set vport fake fail */
	(void)hinic_flush_qp_res(nic_dev->hwdev);
	rte_delay_ms(100);

	hinic_remove_rxtx_configure(dev);

cfg_rxtx_fail:
set_mtu_fail:
cfg_mq_mode_fail:
	hinic_free_qp_ctxts(nic_dev->hwdev);

init_qp_fail:
	hinic_free_all_rx_mbuf(dev);
	hinic_free_all_tx_mbuf(dev);

	return rc;
}

/**
 * DPDK callback to release the receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param qid
 *   Receive queue index.
 */
static void hinic_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct hinic_rxq *rxq = dev->data->rx_queues[qid];
	struct hinic_nic_dev *nic_dev;

	if (!rxq) {
		PMD_DRV_LOG(WARNING, "Rxq is null when release");
		return;
	}
	nic_dev = rxq->nic_dev;

	/* free rxq_pkt mbuf */
	hinic_free_all_rx_mbufs(rxq);

	/* free rxq_cqe, rxq_info */
	hinic_free_rx_resources(rxq);

	/* free root rq wq */
	hinic_destroy_rq(nic_dev->hwdev, rxq->q_id);

	nic_dev->rxqs[rxq->q_id] = NULL;

	/* free rxq */
	rte_free(rxq);
}

/**
 * DPDK callback to release the transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param qid
 *   Transmit queue index.
 */
static void hinic_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct hinic_txq *txq = dev->data->tx_queues[qid];
	struct hinic_nic_dev *nic_dev;

	if (!txq) {
		PMD_DRV_LOG(WARNING, "Txq is null when release");
		return;
	}
	nic_dev = txq->nic_dev;

	/* free txq_pkt mbuf */
	hinic_free_all_tx_mbufs(txq);

	/* free txq_info */
	hinic_free_tx_resources(txq);

	/* free root sq wq */
	hinic_destroy_sq(nic_dev->hwdev, txq->q_id);
	nic_dev->txqs[txq->q_id] = NULL;

	/* free txq */
	rte_free(txq);
}

static void hinic_free_all_rq(struct hinic_nic_dev *nic_dev)
{
	u16 q_id;

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++)
		hinic_destroy_rq(nic_dev->hwdev, q_id);
}

static void hinic_free_all_sq(struct hinic_nic_dev *nic_dev)
{
	u16 q_id;

	for (q_id = 0; q_id < nic_dev->num_sq; q_id++)
		hinic_destroy_sq(nic_dev->hwdev, q_id);
}

/**
 * DPDK callback to stop the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int hinic_dev_stop(struct rte_eth_dev *dev)
{
	int rc;
	char *name;
	uint16_t port_id;
	struct hinic_nic_dev *nic_dev;
	struct rte_eth_link link;
	uint16_t i;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	name = dev->data->name;
	port_id = dev->data->port_id;

	dev->data->dev_started = 0;

	if (!rte_bit_relaxed_test_and_clear32(HINIC_DEV_START,
					      &nic_dev->dev_status)) {
		PMD_DRV_LOG(INFO, "Device %s already stopped", name);
		return 0;
	}

	/* just stop phy port and vport */
	rc = hinic_set_port_enable(nic_dev->hwdev, false);
	if (rc)
		PMD_DRV_LOG(WARNING, "Disable phy port failed, error: %d, dev_name: %s, port_id: %d",
			  rc, name, port_id);

	rc = hinic_set_vport_enable(nic_dev->hwdev, false);
	if (rc)
		PMD_DRV_LOG(WARNING, "Disable vport failed, error: %d, dev_name: %s, port_id: %d",
			  rc, name, port_id);

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	(void)rte_eth_linkstatus_set(dev, &link);

	/* flush pending io request */
	rc = hinic_rx_tx_flush(nic_dev->hwdev);
	if (rc)
		PMD_DRV_LOG(WARNING, "Flush pending io failed, error: %d, dev_name: %s, port_id: %d",
			    rc, name, port_id);

	/* clean rss table and rx_mode */
	hinic_remove_rxtx_configure(dev);

	/* clean root context */
	hinic_free_qp_ctxts(nic_dev->hwdev);

	hinic_destroy_fdir_filter(dev);

	/* free mbuf */
	hinic_free_all_rx_mbuf(dev);
	hinic_free_all_tx_mbuf(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static void hinic_disable_interrupt(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	int ret, retries = 0;

	rte_bit_relaxed_clear32(HINIC_DEV_INTR_EN, &nic_dev->dev_status);

	/* disable msix interrupt in hardware */
	hinic_set_msix_state(nic_dev->hwdev, 0, HINIC_MSIX_DISABLE);

	/* disable rte interrupt */
	ret = rte_intr_disable(pci_dev->intr_handle);
	if (ret)
		PMD_DRV_LOG(ERR, "Disable intr failed: %d", ret);

	do {
		ret =
		rte_intr_callback_unregister(pci_dev->intr_handle,
					     hinic_dev_interrupt_handler, dev);
		if (ret >= 0) {
			break;
		} else if (ret == -EAGAIN) {
			rte_delay_ms(100);
			retries++;
		} else {
			PMD_DRV_LOG(ERR, "intr callback unregister failed: %d",
				    ret);
			break;
		}
	} while (retries < HINIC_INTR_CB_UNREG_MAX_RETRIES);

	if (retries == HINIC_INTR_CB_UNREG_MAX_RETRIES)
		PMD_DRV_LOG(ERR, "Unregister intr callback failed after %d retries",
			    retries);

	rte_bit_relaxed_clear32(HINIC_DEV_INIT, &nic_dev->dev_status);
}

static int hinic_set_dev_promiscuous(struct hinic_nic_dev *nic_dev, bool enable)
{
	u32 rx_mode_ctrl;
	int err;

	err = hinic_mutex_lock(&nic_dev->rx_mode_mutex);
	if (err)
		return err;

	rx_mode_ctrl = nic_dev->rx_mode_status;

	if (enable)
		rx_mode_ctrl |= HINIC_RX_MODE_PROMISC;
	else
		rx_mode_ctrl &= (~HINIC_RX_MODE_PROMISC);

	err = hinic_config_rx_mode(nic_dev, rx_mode_ctrl);

	(void)hinic_mutex_unlock(&nic_dev->rx_mode_mutex);

	return err;
}

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param stats
 *   Stats structure output buffer.
 *
 * @return
 *   0 on success and stats is filled,
 *   negative error value otherwise.
 */
static int
hinic_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	int i, err, q_num;
	u64 rx_discards_pmd = 0;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic_vport_stats vport_stats;
	struct hinic_rxq	*rxq = NULL;
	struct hinic_rxq_stats rxq_stats;
	struct hinic_txq	*txq = NULL;
	struct hinic_txq_stats txq_stats;

	err = hinic_get_vport_stats(nic_dev->hwdev, &vport_stats);
	if (err) {
		PMD_DRV_LOG(ERR, "Get vport stats from fw failed, nic_dev: %s",
			nic_dev->proc_dev_name);
		return err;
	}

	dev->data->rx_mbuf_alloc_failed = 0;

	/* rx queue stats */
	q_num = (nic_dev->num_rq < RTE_ETHDEV_QUEUE_STAT_CNTRS) ?
			nic_dev->num_rq : RTE_ETHDEV_QUEUE_STAT_CNTRS;
	for (i = 0; i < q_num; i++) {
		rxq = nic_dev->rxqs[i];
		hinic_rxq_get_stats(rxq, &rxq_stats);
		stats->q_ipackets[i] = rxq_stats.packets;
		stats->q_ibytes[i] = rxq_stats.bytes;
		stats->q_errors[i] = rxq_stats.rx_discards;

		stats->ierrors += rxq_stats.errors;
		rx_discards_pmd += rxq_stats.rx_discards;
		dev->data->rx_mbuf_alloc_failed += rxq_stats.rx_nombuf;
	}

	/* tx queue stats */
	q_num = (nic_dev->num_sq < RTE_ETHDEV_QUEUE_STAT_CNTRS) ?
		nic_dev->num_sq : RTE_ETHDEV_QUEUE_STAT_CNTRS;
	for (i = 0; i < q_num; i++) {
		txq = nic_dev->txqs[i];
		hinic_txq_get_stats(txq, &txq_stats);
		stats->q_opackets[i] = txq_stats.packets;
		stats->q_obytes[i] = txq_stats.bytes;
		stats->oerrors += (txq_stats.tx_busy + txq_stats.off_errs);
	}

	/* vport stats */
	stats->oerrors += vport_stats.tx_discard_vport;

	stats->imissed = vport_stats.rx_discard_vport + rx_discards_pmd;

	stats->ipackets = (vport_stats.rx_unicast_pkts_vport +
			vport_stats.rx_multicast_pkts_vport +
			vport_stats.rx_broadcast_pkts_vport -
			rx_discards_pmd);

	stats->opackets = (vport_stats.tx_unicast_pkts_vport +
			vport_stats.tx_multicast_pkts_vport +
			vport_stats.tx_broadcast_pkts_vport);

	stats->ibytes = (vport_stats.rx_unicast_bytes_vport +
			vport_stats.rx_multicast_bytes_vport +
			vport_stats.rx_broadcast_bytes_vport);

	stats->obytes = (vport_stats.tx_unicast_bytes_vport +
			vport_stats.tx_multicast_bytes_vport +
			vport_stats.tx_broadcast_bytes_vport);
	return 0;
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int hinic_dev_stats_reset(struct rte_eth_dev *dev)
{
	int qid;
	struct hinic_rxq	*rxq = NULL;
	struct hinic_txq	*txq = NULL;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	ret = hinic_clear_vport_stats(nic_dev->hwdev);
	if (ret != 0)
		return ret;

	for (qid = 0; qid < nic_dev->num_rq; qid++) {
		rxq = nic_dev->rxqs[qid];
		hinic_rxq_stats_reset(rxq);
	}

	for (qid = 0; qid < nic_dev->num_sq; qid++) {
		txq = nic_dev->txqs[qid];
		hinic_txq_stats_reset(txq);
	}

	return 0;
}

/**
 * DPDK callback to clear device extended statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int hinic_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	ret = hinic_dev_stats_reset(dev);
	if (ret != 0)
		return ret;

	if (hinic_func_type(nic_dev->hwdev) != TYPE_VF) {
		ret = hinic_clear_phy_port_stats(nic_dev->hwdev);
		if (ret != 0)
			return ret;
	}

	return 0;
}

static void hinic_gen_random_mac_addr(struct rte_ether_addr *mac_addr)
{
	uint64_t random_value;

	/* Set Organizationally Unique Identifier (OUI) prefix */
	mac_addr->addr_bytes[0] = 0x00;
	mac_addr->addr_bytes[1] = 0x09;
	mac_addr->addr_bytes[2] = 0xC0;
	/* Force indication of locally assigned MAC address. */
	mac_addr->addr_bytes[0] |= RTE_ETHER_LOCAL_ADMIN_ADDR;
	/* Generate the last 3 bytes of the MAC address with a random number. */
	random_value = rte_rand();
	memcpy(&mac_addr->addr_bytes[3], &random_value, 3);
}

/**
 * Init mac_vlan table in NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and stats is filled,
 *   negative error value otherwise.
 */
static int hinic_init_mac_addr(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
	u16 func_id = 0;
	int rc = 0;

	rc = hinic_get_default_mac(nic_dev->hwdev, addr_bytes);
	if (rc)
		return rc;

	rte_ether_addr_copy((struct rte_ether_addr *)addr_bytes,
		&eth_dev->data->mac_addrs[0]);
	if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[0]))
		hinic_gen_random_mac_addr(&eth_dev->data->mac_addrs[0]);

	func_id = hinic_global_func_id(nic_dev->hwdev);
	rc = hinic_set_mac(nic_dev->hwdev,
			eth_dev->data->mac_addrs[0].addr_bytes,
			0, func_id);
	if (rc && rc != HINIC_PF_SET_VF_ALREADY)
		return rc;

	rte_ether_addr_copy(&eth_dev->data->mac_addrs[0],
			&nic_dev->default_addr);

	return 0;
}

static void hinic_delete_mc_addr_list(struct hinic_nic_dev *nic_dev)
{
	u16 func_id;
	u32 i;

	func_id = hinic_global_func_id(nic_dev->hwdev);

	for (i = 0; i < HINIC_MAX_MC_MAC_ADDRS; i++) {
		if (rte_is_zero_ether_addr(&nic_dev->mc_list[i]))
			break;

		hinic_del_mac(nic_dev->hwdev, nic_dev->mc_list[i].addr_bytes,
			      0, func_id);
		memset(&nic_dev->mc_list[i], 0, sizeof(struct rte_ether_addr));
	}
}

/**
 * Deinit mac_vlan table in NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and stats is filled,
 *   negative error value otherwise.
 */
static void hinic_deinit_mac_addr(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	u16 func_id = 0;
	int rc;
	int i;

	func_id = hinic_global_func_id(nic_dev->hwdev);

	for (i = 0; i < HINIC_MAX_UC_MAC_ADDRS; i++) {
		if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[i]))
			continue;

		rc = hinic_del_mac(nic_dev->hwdev,
				   eth_dev->data->mac_addrs[i].addr_bytes,
				   0, func_id);
		if (rc && rc != HINIC_PF_SET_VF_ALREADY)
			PMD_DRV_LOG(ERR, "Delete mac table failed, dev_name: %s",
				    eth_dev->data->name);

		memset(&eth_dev->data->mac_addrs[i], 0,
		       sizeof(struct rte_ether_addr));
	}

	/* delete multicast mac addrs */
	hinic_delete_mc_addr_list(nic_dev);

	rte_free(nic_dev->mc_list);

}

static int hinic_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	PMD_DRV_LOG(INFO, "Set port mtu, port_id: %d, mtu: %d, max_pkt_len: %d",
			dev->data->port_id, mtu, HINIC_MTU_TO_PKTLEN(mtu));

	ret = hinic_set_port_mtu(nic_dev->hwdev, mtu);
	if (ret) {
		PMD_DRV_LOG(ERR, "Set port mtu failed, ret: %d", ret);
		return ret;
	}

	nic_dev->mtu_size = mtu;

	return ret;
}

static void hinic_store_vlan_filter(struct hinic_nic_dev *nic_dev,
					u16 vlan_id, bool on)
{
	u32 vid_idx, vid_bit;

	vid_idx = HINIC_VFTA_IDX(vlan_id);
	vid_bit = HINIC_VFTA_BIT(vlan_id);

	if (on)
		nic_dev->vfta[vid_idx] |= vid_bit;
	else
		nic_dev->vfta[vid_idx] &= ~vid_bit;
}

static bool hinic_find_vlan_filter(struct hinic_nic_dev *nic_dev,
				uint16_t vlan_id)
{
	u32 vid_idx, vid_bit;

	vid_idx = HINIC_VFTA_IDX(vlan_id);
	vid_bit = HINIC_VFTA_BIT(vlan_id);

	return (nic_dev->vfta[vid_idx] & vid_bit) ? TRUE : FALSE;
}

/**
 * DPDK callback to set vlan filter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param vlan_id
 *   vlan id is used to filter vlan packets
 * @param enable
 *   enable disable or enable vlan filter function
 */
static int hinic_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id, int enable)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int err = 0;
	u16 func_id;

	if (vlan_id > RTE_ETHER_MAX_VLAN_ID)
		return -EINVAL;

	if (vlan_id == 0)
		return 0;

	func_id = hinic_global_func_id(nic_dev->hwdev);

	if (enable) {
		/* If vlanid is already set, just return */
		if (hinic_find_vlan_filter(nic_dev, vlan_id)) {
			PMD_DRV_LOG(INFO, "Vlan %u has been added, device: %s",
				  vlan_id, nic_dev->proc_dev_name);
			return 0;
		}

		err = hinic_add_remove_vlan(nic_dev->hwdev, vlan_id,
					    func_id, TRUE);
	} else {
		/* If vlanid can't be found, just return */
		if (!hinic_find_vlan_filter(nic_dev, vlan_id)) {
			PMD_DRV_LOG(INFO, "Vlan %u is not in the vlan filter list, device: %s",
				  vlan_id, nic_dev->proc_dev_name);
			return 0;
		}

		err = hinic_add_remove_vlan(nic_dev->hwdev, vlan_id,
					    func_id, FALSE);
	}

	if (err) {
		PMD_DRV_LOG(ERR, "%s vlan failed, func_id: %d, vlan_id: %d, err: %d",
		      enable ? "Add" : "Remove", func_id, vlan_id, err);
		return err;
	}

	hinic_store_vlan_filter(nic_dev, vlan_id, enable);

	PMD_DRV_LOG(INFO, "%s vlan %u succeed, device: %s",
		  enable ? "Add" : "Remove", vlan_id, nic_dev->proc_dev_name);
	return 0;
}

/**
 * DPDK callback to enable or disable vlan offload.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mask
 *   Definitions used for VLAN setting
 */
static int hinic_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	bool on;
	int err;

	/* Enable or disable VLAN filter */
	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		on = (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) ?
			TRUE : FALSE;
		err = hinic_config_vlan_filter(nic_dev->hwdev, on);
		if (err == HINIC_MGMT_CMD_UNSUPPORTED) {
			PMD_DRV_LOG(WARNING,
				"Current matching version does not support vlan filter configuration, device: %s, port_id: %d",
				  nic_dev->proc_dev_name, dev->data->port_id);
		} else if (err) {
			PMD_DRV_LOG(ERR, "Failed to %s vlan filter, device: %s, port_id: %d, err: %d",
				  on ? "enable" : "disable",
				  nic_dev->proc_dev_name,
				  dev->data->port_id, err);
			return err;
		}

		PMD_DRV_LOG(INFO, "%s vlan filter succeed, device: %s, port_id: %d",
			  on ? "Enable" : "Disable",
			  nic_dev->proc_dev_name, dev->data->port_id);
	}

	/* Enable or disable VLAN stripping */
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		on = (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) ?
			TRUE : FALSE;
		err = hinic_set_rx_vlan_offload(nic_dev->hwdev, on);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to %s vlan strip, device: %s, port_id: %d, err: %d",
				  on ? "enable" : "disable",
				  nic_dev->proc_dev_name,
				  dev->data->port_id, err);
			return err;
		}

		PMD_DRV_LOG(INFO, "%s vlan strip succeed, device: %s, port_id: %d",
			  on ? "Enable" : "Disable",
			  nic_dev->proc_dev_name, dev->data->port_id);
	}

	return 0;
}

static void hinic_remove_all_vlanid(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
		HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	u16 func_id;
	int i;

	func_id = hinic_global_func_id(nic_dev->hwdev);
	for (i = 0; i <= RTE_ETHER_MAX_VLAN_ID; i++) {
		/* If can't find it, continue */
		if (!hinic_find_vlan_filter(nic_dev, i))
			continue;

		(void)hinic_add_remove_vlan(nic_dev->hwdev, i, func_id, FALSE);
		hinic_store_vlan_filter(nic_dev, i, false);
	}
}

static int hinic_set_dev_allmulticast(struct hinic_nic_dev *nic_dev,
				bool enable)
{
	u32 rx_mode_ctrl;
	int err;

	err = hinic_mutex_lock(&nic_dev->rx_mode_mutex);
	if (err)
		return err;

	rx_mode_ctrl = nic_dev->rx_mode_status;

	if (enable)
		rx_mode_ctrl |= HINIC_RX_MODE_MC_ALL;
	else
		rx_mode_ctrl &= (~HINIC_RX_MODE_MC_ALL);

	err = hinic_config_rx_mode(nic_dev, rx_mode_ctrl);

	(void)hinic_mutex_unlock(&nic_dev->rx_mode_mutex);

	return err;
}

/**
 * DPDK callback to enable allmulticast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
static int hinic_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	int ret = HINIC_OK;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	ret = hinic_set_dev_allmulticast(nic_dev, true);
	if (ret) {
		PMD_DRV_LOG(ERR, "Enable allmulticast failed, error: %d", ret);
		return ret;
	}

	PMD_DRV_LOG(INFO, "Enable allmulticast succeed, nic_dev: %s, port_id: %d",
		nic_dev->proc_dev_name, dev->data->port_id);
	return 0;
}

/**
 * DPDK callback to disable allmulticast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
static int hinic_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	int ret = HINIC_OK;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	ret = hinic_set_dev_allmulticast(nic_dev, false);
	if (ret) {
		PMD_DRV_LOG(ERR, "Disable allmulticast failed, error: %d", ret);
		return ret;
	}

	PMD_DRV_LOG(INFO, "Disable allmulticast succeed, nic_dev: %s, port_id: %d",
		nic_dev->proc_dev_name, dev->data->port_id);
	return 0;
}

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
static int hinic_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	int rc = HINIC_OK;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	PMD_DRV_LOG(INFO, "Enable promiscuous, nic_dev: %s, port_id: %d, promisc: %d",
		    nic_dev->proc_dev_name, dev->data->port_id,
		    dev->data->promiscuous);

	rc = hinic_set_dev_promiscuous(nic_dev, true);
	if (rc)
		PMD_DRV_LOG(ERR, "Enable promiscuous failed");

	return rc;
}

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
static int hinic_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	int rc = HINIC_OK;
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	PMD_DRV_LOG(INFO, "Disable promiscuous, nic_dev: %s, port_id: %d, promisc: %d",
		    nic_dev->proc_dev_name, dev->data->port_id,
		    dev->data->promiscuous);

	rc = hinic_set_dev_promiscuous(nic_dev, false);
	if (rc)
		PMD_DRV_LOG(ERR, "Disable promiscuous failed");

	return rc;
}

static int hinic_flow_ctrl_get(struct rte_eth_dev *dev,
			struct rte_eth_fc_conf *fc_conf)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct nic_pause_config nic_pause;
	int err;

	memset(&nic_pause, 0, sizeof(nic_pause));

	err = hinic_get_pause_info(nic_dev->hwdev, &nic_pause);
	if (err)
		return err;

	if (nic_dev->pause_set || !nic_pause.auto_neg) {
		nic_pause.rx_pause = nic_dev->nic_pause.rx_pause;
		nic_pause.tx_pause = nic_dev->nic_pause.tx_pause;
	}

	fc_conf->autoneg = nic_pause.auto_neg;

	if (nic_pause.tx_pause && nic_pause.rx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (nic_pause.tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else if (nic_pause.rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;

	return 0;
}

static int hinic_flow_ctrl_set(struct rte_eth_dev *dev,
			struct rte_eth_fc_conf *fc_conf)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct nic_pause_config nic_pause;
	int err;

	nic_pause.auto_neg = fc_conf->autoneg;

	if (((fc_conf->mode & RTE_ETH_FC_FULL) == RTE_ETH_FC_FULL) ||
		(fc_conf->mode & RTE_ETH_FC_TX_PAUSE))
		nic_pause.tx_pause = true;
	else
		nic_pause.tx_pause = false;

	if (((fc_conf->mode & RTE_ETH_FC_FULL) == RTE_ETH_FC_FULL) ||
		(fc_conf->mode & RTE_ETH_FC_RX_PAUSE))
		nic_pause.rx_pause = true;
	else
		nic_pause.rx_pause = false;

	err = hinic_set_pause_config(nic_dev->hwdev, nic_pause);
	if (err)
		return err;

	nic_dev->pause_set = true;
	nic_dev->nic_pause.auto_neg = nic_pause.auto_neg;
	nic_dev->nic_pause.rx_pause = nic_pause.rx_pause;
	nic_dev->nic_pause.tx_pause = nic_pause.tx_pause;

	PMD_DRV_LOG(INFO, "Set pause options, tx: %s, rx: %s, auto: %s\n",
		nic_pause.tx_pause ? "on" : "off",
		nic_pause.rx_pause ? "on" : "off",
		nic_pause.auto_neg ? "on" : "off");

	return 0;
}

/**
 * DPDK callback to update the RSS hash key and RSS hash type.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rss_conf
 *   RSS configuration data.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u8 tmpl_idx = nic_dev->rss_tmpl_idx;
	u8 hashkey[HINIC_RSS_KEY_SIZE] = {0};
	u8 prio_tc[HINIC_DCB_UP_MAX] = {0};
	u64 rss_hf = rss_conf->rss_hf;
	struct nic_rss_type rss_type = {0};
	int err = 0;

	if (!(nic_dev->flags & RTE_ETH_MQ_RX_RSS_FLAG)) {
		PMD_DRV_LOG(WARNING, "RSS is not enabled");
		return HINIC_OK;
	}

	if (rss_conf->rss_key_len > HINIC_RSS_KEY_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid rss key, rss_key_len: %d",
			    rss_conf->rss_key_len);
		return HINIC_ERROR;
	}

	if (rss_conf->rss_key) {
		memcpy(hashkey, rss_conf->rss_key, rss_conf->rss_key_len);
		err = hinic_rss_set_template_tbl(nic_dev->hwdev, tmpl_idx,
						 hashkey);
		if (err) {
			PMD_DRV_LOG(ERR, "Set rss template table failed");
			goto disable_rss;
		}
	}

	rss_type.ipv4 = (rss_hf & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4)) ? 1 : 0;
	rss_type.tcp_ipv4 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) ? 1 : 0;
	rss_type.ipv6 = (rss_hf & (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6)) ? 1 : 0;
	rss_type.ipv6_ext = (rss_hf & RTE_ETH_RSS_IPV6_EX) ? 1 : 0;
	rss_type.tcp_ipv6 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) ? 1 : 0;
	rss_type.tcp_ipv6_ext = (rss_hf & RTE_ETH_RSS_IPV6_TCP_EX) ? 1 : 0;
	rss_type.udp_ipv4 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) ? 1 : 0;
	rss_type.udp_ipv6 = (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) ? 1 : 0;

	err = hinic_set_rss_type(nic_dev->hwdev, tmpl_idx, rss_type);
	if (err) {
		PMD_DRV_LOG(ERR, "Set rss type table failed");
		goto disable_rss;
	}

	return 0;

disable_rss:
	memset(prio_tc, 0, sizeof(prio_tc));
	(void)hinic_rss_cfg(nic_dev->hwdev, 0, tmpl_idx, 0, prio_tc);
	return err;
}

/**
 * DPDK callback to get the RSS hash configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rss_conf
 *   RSS configuration data.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_rss_conf_get(struct rte_eth_dev *dev,
		       struct rte_eth_rss_conf *rss_conf)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u8 tmpl_idx = nic_dev->rss_tmpl_idx;
	u8 hashkey[HINIC_RSS_KEY_SIZE] = {0};
	struct nic_rss_type rss_type = {0};
	int err;

	if (!(nic_dev->flags & RTE_ETH_MQ_RX_RSS_FLAG)) {
		PMD_DRV_LOG(WARNING, "RSS is not enabled");
		return HINIC_ERROR;
	}

	err = hinic_rss_get_template_tbl(nic_dev->hwdev, tmpl_idx, hashkey);
	if (err)
		return err;

	if (rss_conf->rss_key &&
	    rss_conf->rss_key_len >= HINIC_RSS_KEY_SIZE) {
		memcpy(rss_conf->rss_key, hashkey, sizeof(hashkey));
		rss_conf->rss_key_len = sizeof(hashkey);
	}

	err = hinic_get_rss_type(nic_dev->hwdev, tmpl_idx, &rss_type);
	if (err)
		return err;

	rss_conf->rss_hf = 0;
	rss_conf->rss_hf |=  rss_type.ipv4 ?
		(RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4) : 0;
	rss_conf->rss_hf |=  rss_type.tcp_ipv4 ? RTE_ETH_RSS_NONFRAG_IPV4_TCP : 0;
	rss_conf->rss_hf |=  rss_type.ipv6 ?
		(RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6) : 0;
	rss_conf->rss_hf |=  rss_type.ipv6_ext ? RTE_ETH_RSS_IPV6_EX : 0;
	rss_conf->rss_hf |=  rss_type.tcp_ipv6 ? RTE_ETH_RSS_NONFRAG_IPV6_TCP : 0;
	rss_conf->rss_hf |=  rss_type.tcp_ipv6_ext ? RTE_ETH_RSS_IPV6_TCP_EX : 0;
	rss_conf->rss_hf |=  rss_type.udp_ipv4 ? RTE_ETH_RSS_NONFRAG_IPV4_UDP : 0;
	rss_conf->rss_hf |=  rss_type.udp_ipv6 ? RTE_ETH_RSS_NONFRAG_IPV6_UDP : 0;

	return HINIC_OK;
}

/**
 * DPDK callback to update the RSS redirection table.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param reta_conf
 *   Pointer to RSS reta configuration data.
 * @param reta_size
 *   Size of the RETA table.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_rss_indirtbl_update(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u8 tmpl_idx = nic_dev->rss_tmpl_idx;
	u8 prio_tc[HINIC_DCB_UP_MAX] = {0};
	u32 indirtbl[NIC_RSS_INDIR_SIZE] = {0};
	int err = 0;
	u16 i = 0;
	u16 idx, shift;

	if (!(nic_dev->flags & RTE_ETH_MQ_RX_RSS_FLAG))
		return HINIC_OK;

	if (reta_size != NIC_RSS_INDIR_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid reta size, reta_size: %d", reta_size);
		return HINIC_ERROR;
	}

	err = hinic_rss_get_indir_tbl(nic_dev->hwdev, tmpl_idx, indirtbl);
	if (err)
		return err;

	/* update rss indir_tbl */
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;

		if (reta_conf[idx].reta[shift] >= nic_dev->num_rq) {
			PMD_DRV_LOG(ERR, "Invalid reta entry, indirtbl[%d]: %d "
				"exceeds the maximum rxq num: %d", i,
				reta_conf[idx].reta[shift], nic_dev->num_rq);
			return -EINVAL;
		}

		if (reta_conf[idx].mask & (1ULL << shift))
			indirtbl[i] = reta_conf[idx].reta[shift];
	}

	err = hinic_rss_set_indir_tbl(nic_dev->hwdev, tmpl_idx, indirtbl);
	if (err)
		goto disable_rss;

	nic_dev->rss_indir_flag = true;

	return 0;

disable_rss:
	memset(prio_tc, 0, sizeof(prio_tc));
	(void)hinic_rss_cfg(nic_dev->hwdev, 0, tmpl_idx, 0, prio_tc);

	return HINIC_ERROR;
}

/**
 * DPDK callback to get the RSS indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param reta_conf
 *   Pointer to RSS reta configuration data.
 * @param reta_size
 *   Size of the RETA table.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_rss_indirtbl_query(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u8 tmpl_idx = nic_dev->rss_tmpl_idx;
	int err = 0;
	u32 indirtbl[NIC_RSS_INDIR_SIZE] = {0};
	u16 idx, shift;
	u16 i = 0;

	if (reta_size != NIC_RSS_INDIR_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid reta size, reta_size: %d", reta_size);
		return HINIC_ERROR;
	}

	err = hinic_rss_get_indir_tbl(nic_dev->hwdev, tmpl_idx, indirtbl);
	if (err) {
		PMD_DRV_LOG(ERR, "Get rss indirect table failed, error: %d",
			    err);
		return err;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = (uint16_t)indirtbl[i];
	}

	return HINIC_OK;
}

/**
 * DPDK callback to get extended device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param xstats
 *   Pointer to rte extended stats table.
 * @param n
 *   The size of the stats table.
 *
 * @return
 *   Number of extended stats on success and stats is filled,
 *   negative error value otherwise.
 */
static int hinic_dev_xstats_get(struct rte_eth_dev *dev,
			 struct rte_eth_xstat *xstats,
			 unsigned int n)
{
	u16 qid = 0;
	u32 i;
	int err, count;
	struct hinic_nic_dev *nic_dev;
	struct hinic_phy_port_stats port_stats;
	struct hinic_vport_stats vport_stats;
	struct hinic_rxq	*rxq = NULL;
	struct hinic_rxq_stats rxq_stats;
	struct hinic_txq	*txq = NULL;
	struct hinic_txq_stats txq_stats;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	count = hinic_xstats_calc_num(nic_dev);
	if ((int)n < count)
		return count;

	count = 0;

	/* Get stats from hinic_rxq_stats */
	for (qid = 0; qid < nic_dev->num_rq; qid++) {
		rxq = nic_dev->rxqs[qid];
		hinic_rxq_get_stats(rxq, &rxq_stats);

		for (i = 0; i < HINIC_RXQ_XSTATS_NUM; i++) {
			xstats[count].value =
				*(uint64_t *)(((char *)&rxq_stats) +
				hinic_rxq_stats_strings[i].offset);
			xstats[count].id = count;
			count++;
		}
	}

	/* Get stats from hinic_txq_stats */
	for (qid = 0; qid < nic_dev->num_sq; qid++) {
		txq = nic_dev->txqs[qid];
		hinic_txq_get_stats(txq, &txq_stats);

		for (i = 0; i < HINIC_TXQ_XSTATS_NUM; i++) {
			xstats[count].value =
				*(uint64_t *)(((char *)&txq_stats) +
				hinic_txq_stats_strings[i].offset);
			xstats[count].id = count;
			count++;
		}
	}

	/* Get stats from hinic_vport_stats */
	err = hinic_get_vport_stats(nic_dev->hwdev, &vport_stats);
	if (err)
		return err;

	for (i = 0; i < HINIC_VPORT_XSTATS_NUM; i++) {
		xstats[count].value =
			*(uint64_t *)(((char *)&vport_stats) +
			hinic_vport_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	if (HINIC_IS_VF(nic_dev->hwdev))
		return count;

	/* Get stats from hinic_phy_port_stats */
	err = hinic_get_phy_port_stats(nic_dev->hwdev, &port_stats);
	if (err)
		return err;

	for (i = 0; i < HINIC_PHYPORT_XSTATS_NUM; i++) {
		xstats[count].value = *(uint64_t *)(((char *)&port_stats) +
				hinic_phyport_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	return count;
}

static void hinic_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
				struct rte_eth_rxq_info *qinfo)
{
	struct hinic_rxq  *rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->nb_desc = rxq->q_depth;
}

static void hinic_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
				struct rte_eth_txq_info *qinfo)
{
	struct hinic_txq  *txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->q_depth;
}

/**
 * DPDK callback to retrieve names of extended device statistics
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param xstats_names
 *   Buffer to insert names into.
 *
 * @return
 *   Number of xstats names.
 */
static int hinic_dev_xstats_get_names(struct rte_eth_dev *dev,
			       struct rte_eth_xstat_name *xstats_names,
			       __rte_unused unsigned int limit)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int count = 0;
	u16 i = 0, q_num;

	if (xstats_names == NULL)
		return hinic_xstats_calc_num(nic_dev);

	/* get pmd rxq stats */
	for (q_num = 0; q_num < nic_dev->num_rq; q_num++) {
		for (i = 0; i < HINIC_RXQ_XSTATS_NUM; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rxq%d_%s_pmd",
				 q_num, hinic_rxq_stats_strings[i].name);
			count++;
		}
	}

	/* get pmd txq stats */
	for (q_num = 0; q_num < nic_dev->num_sq; q_num++) {
		for (i = 0; i < HINIC_TXQ_XSTATS_NUM; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "txq%d_%s_pmd",
				 q_num, hinic_txq_stats_strings[i].name);
			count++;
		}
	}

	/* get vport stats */
	for (i = 0; i < HINIC_VPORT_XSTATS_NUM; i++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "%s", hinic_vport_stats_strings[i].name);
		count++;
	}

	if (HINIC_IS_VF(nic_dev->hwdev))
		return count;

	/* get phy port stats */
	for (i = 0; i < HINIC_PHYPORT_XSTATS_NUM; i++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "%s", hinic_phyport_stats_strings[i].name);
		count++;
	}

	return count;
}

/**
 *  DPDK callback to set mac address
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param addr
 *   Pointer to mac address
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_set_mac_addr(struct rte_eth_dev *dev,
			      struct rte_ether_addr *addr)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u16 func_id;
	int err;

	func_id = hinic_global_func_id(nic_dev->hwdev);
	err = hinic_update_mac(nic_dev->hwdev, nic_dev->default_addr.addr_bytes,
			       addr->addr_bytes, 0, func_id);
	if (err)
		return err;

	rte_ether_addr_copy(addr, &nic_dev->default_addr);

	PMD_DRV_LOG(INFO, "Set new mac address " RTE_ETHER_ADDR_PRT_FMT,
		    RTE_ETHER_ADDR_BYTES(addr));

	return 0;
}

/**
 * DPDK callback to remove a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index, should less than 128.
 */
static void hinic_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u16 func_id;
	int ret;

	if (index >= HINIC_MAX_UC_MAC_ADDRS) {
		PMD_DRV_LOG(INFO, "Remove mac index(%u) is out of range",
			    index);
		return;
	}

	func_id = hinic_global_func_id(nic_dev->hwdev);
	ret = hinic_del_mac(nic_dev->hwdev,
			    dev->data->mac_addrs[index].addr_bytes, 0, func_id);
	if (ret)
		return;

	memset(&dev->data->mac_addrs[index], 0, sizeof(struct rte_ether_addr));
}

/**
 * DPDK callback to add a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   Pointer to MAC address
 * @param index
 *   MAC address index, should less than 128.
 * @param vmdq
 *   VMDq pool index(not used).
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_mac_addr_add(struct rte_eth_dev *dev,
			      struct rte_ether_addr *mac_addr, uint32_t index,
			      __rte_unused uint32_t vmdq)
{
	struct hinic_nic_dev  *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	unsigned int i;
	u16 func_id;
	int ret;

	if (index >= HINIC_MAX_UC_MAC_ADDRS) {
		PMD_DRV_LOG(INFO, "Add mac index(%u) is out of range", index);
		return -EINVAL;
	}

	/* First, make sure this address isn't already configured. */
	for (i = 0; (i != HINIC_MAX_UC_MAC_ADDRS); ++i) {
		/* Skip this index, it's going to be reconfigured. */
		if (i == index)
			continue;

		if (memcmp(&dev->data->mac_addrs[i],
			mac_addr, sizeof(*mac_addr)))
			continue;

		PMD_DRV_LOG(INFO, "MAC address already configured");
		return -EADDRINUSE;
	}

	func_id = hinic_global_func_id(nic_dev->hwdev);
	ret = hinic_set_mac(nic_dev->hwdev, mac_addr->addr_bytes, 0, func_id);
	if (ret)
		return ret;

	dev->data->mac_addrs[index] = *mac_addr;
	return 0;
}

/**
 *  DPDK callback to set multicast mac address
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mc_addr_set
 *   Pointer to multicast mac address
 * @param nb_mc_addr
 *   mc addr count
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_set_mc_addr_list(struct rte_eth_dev *dev,
				  struct rte_ether_addr *mc_addr_set,
				  uint32_t nb_mc_addr)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u16 func_id;
	int ret;
	u32 i;

	func_id = hinic_global_func_id(nic_dev->hwdev);

	/* delete old multi_cast addrs firstly */
	hinic_delete_mc_addr_list(nic_dev);

	if (nb_mc_addr > HINIC_MAX_MC_MAC_ADDRS)
		goto allmulti;

	for (i = 0; i < nb_mc_addr; i++) {
		ret = hinic_set_mac(nic_dev->hwdev, mc_addr_set[i].addr_bytes,
				    0, func_id);
		/* if add mc addr failed, set all multi_cast */
		if (ret) {
			hinic_delete_mc_addr_list(nic_dev);
			goto allmulti;
		}

		rte_ether_addr_copy(&mc_addr_set[i], &nic_dev->mc_list[i]);
	}

	return 0;

allmulti:
	hinic_dev_allmulticast_enable(dev);

	return 0;
}

/**
 * DPDK callback to get flow operations
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param ops
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int hinic_dev_flow_ops_get(struct rte_eth_dev *dev __rte_unused,
				  const struct rte_flow_ops **ops)
{
	*ops = &hinic_flow_ops;
	return 0;
}

static int hinic_set_default_pause_feature(struct hinic_nic_dev *nic_dev)
{
	struct nic_pause_config pause_config = {0};
	int err;

	pause_config.auto_neg = 0;
	pause_config.rx_pause = HINIC_DEFAUT_PAUSE_CONFIG;
	pause_config.tx_pause = HINIC_DEFAUT_PAUSE_CONFIG;

	err = hinic_set_pause_config(nic_dev->hwdev, pause_config);
	if (err)
		return err;

	nic_dev->pause_set = true;
	nic_dev->nic_pause.auto_neg = pause_config.auto_neg;
	nic_dev->nic_pause.rx_pause = pause_config.rx_pause;
	nic_dev->nic_pause.tx_pause = pause_config.tx_pause;

	return 0;
}

static int hinic_set_default_dcb_feature(struct hinic_nic_dev *nic_dev)
{
	u8 up_tc[HINIC_DCB_UP_MAX] = {0};
	u8 up_pgid[HINIC_DCB_UP_MAX] = {0};
	u8 up_bw[HINIC_DCB_UP_MAX] = {0};
	u8 pg_bw[HINIC_DCB_UP_MAX] = {0};
	u8 up_strict[HINIC_DCB_UP_MAX] = {0};
	int i = 0;

	pg_bw[0] = 100;
	for (i = 0; i < HINIC_DCB_UP_MAX; i++)
		up_bw[i] = 100;

	return hinic_dcb_set_ets(nic_dev->hwdev, up_tc, pg_bw,
					up_pgid, up_bw, up_strict);
}

static int hinic_pf_get_default_cos(struct hinic_hwdev *hwdev, u8 *cos_id)
{
	u8 default_cos = 0;
	u8 valid_cos_bitmap;
	u8 i;

	valid_cos_bitmap = hwdev->cfg_mgmt->svc_cap.valid_cos_bitmap;
	if (!valid_cos_bitmap) {
		PMD_DRV_LOG(ERR, "PF has none cos to support\n");
		return -EFAULT;
	}

	for (i = 0; i < NR_MAX_COS; i++) {
		if (valid_cos_bitmap & BIT(i))
			default_cos = i; /* Find max cos id as default cos */
	}

	*cos_id = default_cos;

	return 0;
}

static int hinic_init_default_cos(struct hinic_nic_dev *nic_dev)
{
	u8 cos_id = 0;
	int err;

	if (!HINIC_IS_VF(nic_dev->hwdev)) {
		err = hinic_pf_get_default_cos(nic_dev->hwdev, &cos_id);
		if (err) {
			PMD_DRV_LOG(ERR, "Get PF default cos failed, err: %d",
				    err);
			return HINIC_ERROR;
		}
	} else {
		err = hinic_vf_get_default_cos(nic_dev->hwdev, &cos_id);
		if (err) {
			PMD_DRV_LOG(ERR, "Get VF default cos failed, err: %d",
				    err);
			return HINIC_ERROR;
		}
	}

	nic_dev->default_cos = cos_id;

	PMD_DRV_LOG(INFO, "Default cos %d", nic_dev->default_cos);

	return 0;
}

static int hinic_set_default_hw_feature(struct hinic_nic_dev *nic_dev)
{
	int err;

	err = hinic_init_default_cos(nic_dev);
	if (err)
		return err;

	if (hinic_func_type(nic_dev->hwdev) == TYPE_VF)
		return 0;

	/* Restore DCB configure to default status */
	err = hinic_set_default_dcb_feature(nic_dev);
	if (err)
		return err;

	/* Set pause enable, and up will disable pfc. */
	err = hinic_set_default_pause_feature(nic_dev);
	if (err)
		return err;

	err = hinic_reset_port_link_cfg(nic_dev->hwdev);
	if (err)
		return err;

	err = hinic_set_link_status_follow(nic_dev->hwdev,
					   HINIC_LINK_FOLLOW_PORT);
	if (err == HINIC_MGMT_CMD_UNSUPPORTED)
		PMD_DRV_LOG(WARNING, "Don't support to set link status follow phy port status");
	else if (err)
		return err;

	return hinic_set_anti_attack(nic_dev->hwdev, true);
}

static int32_t hinic_card_workmode_check(struct hinic_nic_dev *nic_dev)
{
	struct hinic_board_info info = { 0 };
	int rc;

	if (hinic_func_type(nic_dev->hwdev) == TYPE_VF)
		return 0;

	rc = hinic_get_board_info(nic_dev->hwdev, &info);
	if (rc)
		return rc;

	return (info.service_mode == HINIC_SERVICE_MODE_NIC ? HINIC_OK :
						HINIC_ERROR);
}

static int hinic_copy_mempool_init(struct hinic_nic_dev *nic_dev)
{
	nic_dev->cpy_mpool = rte_mempool_lookup(nic_dev->proc_dev_name);
	if (nic_dev->cpy_mpool == NULL) {
		nic_dev->cpy_mpool =
		rte_pktmbuf_pool_create(nic_dev->proc_dev_name,
					HINIC_COPY_MEMPOOL_DEPTH,
					0, 0,
					HINIC_COPY_MBUF_SIZE,
					rte_socket_id());
		if (!nic_dev->cpy_mpool) {
			PMD_DRV_LOG(ERR, "Create copy mempool failed, errno: %d, dev_name: %s",
				    rte_errno, nic_dev->proc_dev_name);
			return -ENOMEM;
		}
	}

	return 0;
}

static void hinic_copy_mempool_uninit(struct hinic_nic_dev *nic_dev)
{
	rte_mempool_free(nic_dev->cpy_mpool);
}

static int hinic_init_sw_rxtxqs(struct hinic_nic_dev *nic_dev)
{
	u32 txq_size;
	u32 rxq_size;

	/* allocate software txq array */
	txq_size = nic_dev->nic_cap.max_sqs * sizeof(*nic_dev->txqs);
	nic_dev->txqs = kzalloc_aligned(txq_size, GFP_KERNEL);
	if (!nic_dev->txqs) {
		PMD_DRV_LOG(ERR, "Allocate txqs failed");
		return -ENOMEM;
	}

	/* allocate software rxq array */
	rxq_size = nic_dev->nic_cap.max_rqs * sizeof(*nic_dev->rxqs);
	nic_dev->rxqs = kzalloc_aligned(rxq_size, GFP_KERNEL);
	if (!nic_dev->rxqs) {
		/* free txqs */
		kfree(nic_dev->txqs);
		nic_dev->txqs = NULL;

		PMD_DRV_LOG(ERR, "Allocate rxqs failed");
		return -ENOMEM;
	}

	return HINIC_OK;
}

static void hinic_deinit_sw_rxtxqs(struct hinic_nic_dev *nic_dev)
{
	kfree(nic_dev->txqs);
	nic_dev->txqs = NULL;

	kfree(nic_dev->rxqs);
	nic_dev->rxqs = NULL;
}

static int hinic_nic_dev_create(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	int rc;

	nic_dev->hwdev = rte_zmalloc("hinic_hwdev", sizeof(*nic_dev->hwdev),
				     RTE_CACHE_LINE_SIZE);
	if (!nic_dev->hwdev) {
		PMD_DRV_LOG(ERR, "Allocate hinic hwdev memory failed, dev_name: %s",
			    eth_dev->data->name);
		return -ENOMEM;
	}
	nic_dev->hwdev->pcidev_hdl = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* init osdep*/
	rc = hinic_osdep_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize os_dep failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_osdep_fail;
	}

	/* init_hwif */
	rc = hinic_hwif_res_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize hwif failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_hwif_fail;
	}

	/* init_cfg_mgmt */
	rc = init_cfg_mgmt(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize cfg_mgmt failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_cfgmgnt_fail;
	}

	/* init_aeqs */
	rc = hinic_comm_aeqs_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize aeqs failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_aeqs_fail;
	}

	/* init_pf_to_mgnt */
	rc = hinic_comm_pf_to_mgmt_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize pf_to_mgmt failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_pf_to_mgmt_fail;
	}

	/* init mailbox */
	rc = hinic_comm_func_to_func_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize func_to_func failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_func_to_func_fail;
	}

	rc = hinic_card_workmode_check(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Check card workmode failed, dev_name: %s",
			    eth_dev->data->name);
		goto workmode_check_fail;
	}

	/* do l2nic reset to make chip clear */
	rc = hinic_l2nic_reset(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Do l2nic reset failed, dev_name: %s",
			    eth_dev->data->name);
		goto l2nic_reset_fail;
	}

	/* init dma and aeq msix attribute table */
	(void)hinic_init_attr_table(nic_dev->hwdev);

	/* init_cmdqs */
	rc = hinic_comm_cmdqs_init(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize cmdq failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_cmdq_fail;
	}

	/* set hardware state active */
	rc = hinic_activate_hwdev_state(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize resources state failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_resources_state_fail;
	}

	/* init_capability */
	rc = hinic_init_capability(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize capability failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_cap_fail;
	}

	/* get nic capability */
	if (!hinic_support_nic(nic_dev->hwdev, &nic_dev->nic_cap)) {
		PMD_DRV_LOG(ERR, "Hw doesn't support nic, dev_name: %s",
			    eth_dev->data->name);
		rc = -EINVAL;
		goto nic_check_fail;
	}

	/* init root cla and function table */
	rc = hinic_init_nicio(nic_dev->hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize nic_io failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_nicio_fail;
	}

	/* init_software_txrxq */
	rc = hinic_init_sw_rxtxqs(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize sw_rxtxqs failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_sw_rxtxqs_fail;
	}

	rc = hinic_copy_mempool_init(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Create copy mempool failed, dev_name: %s",
			 eth_dev->data->name);
		goto init_mpool_fail;
	}

	/* set hardware feature to default status */
	rc = hinic_set_default_hw_feature(nic_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize hardware default features failed, dev_name: %s",
			    eth_dev->data->name);
		goto set_default_hw_feature_fail;
	}

	return 0;

set_default_hw_feature_fail:
	hinic_copy_mempool_uninit(nic_dev);

init_mpool_fail:
	hinic_deinit_sw_rxtxqs(nic_dev);

init_sw_rxtxqs_fail:
	hinic_deinit_nicio(nic_dev->hwdev);

nic_check_fail:
init_nicio_fail:
init_cap_fail:
	hinic_deactivate_hwdev_state(nic_dev->hwdev);

init_resources_state_fail:
	hinic_comm_cmdqs_free(nic_dev->hwdev);

init_cmdq_fail:
l2nic_reset_fail:
workmode_check_fail:
	hinic_comm_func_to_func_free(nic_dev->hwdev);

init_func_to_func_fail:
	hinic_comm_pf_to_mgmt_free(nic_dev->hwdev);

init_pf_to_mgmt_fail:
	hinic_comm_aeqs_free(nic_dev->hwdev);

init_aeqs_fail:
	free_cfg_mgmt(nic_dev->hwdev);

init_cfgmgnt_fail:
	hinic_hwif_res_free(nic_dev->hwdev);

init_hwif_fail:
	hinic_osdep_deinit(nic_dev->hwdev);

init_osdep_fail:
	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;

	return rc;
}

static void hinic_nic_dev_destroy(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
			HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	(void)hinic_set_link_status_follow(nic_dev->hwdev,
					   HINIC_LINK_FOLLOW_DEFAULT);
	hinic_copy_mempool_uninit(nic_dev);
	hinic_deinit_sw_rxtxqs(nic_dev);
	hinic_deinit_nicio(nic_dev->hwdev);
	hinic_deactivate_hwdev_state(nic_dev->hwdev);
	hinic_comm_cmdqs_free(nic_dev->hwdev);
	hinic_comm_func_to_func_free(nic_dev->hwdev);
	hinic_comm_pf_to_mgmt_free(nic_dev->hwdev);
	hinic_comm_aeqs_free(nic_dev->hwdev);
	free_cfg_mgmt(nic_dev->hwdev);
	hinic_hwif_res_free(nic_dev->hwdev);
	hinic_osdep_deinit(nic_dev->hwdev);
	rte_free(nic_dev->hwdev);
	nic_dev->hwdev = NULL;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int hinic_dev_close(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (rte_bit_relaxed_test_and_set32(HINIC_DEV_CLOSE,
					   &nic_dev->dev_status)) {
		PMD_DRV_LOG(WARNING, "Device %s already closed",
			    dev->data->name);
		return 0;
	}

	/* stop device first */
	ret = hinic_dev_stop(dev);

	/* rx_cqe, rx_info */
	hinic_free_all_rx_resources(dev);

	/* tx_info */
	hinic_free_all_tx_resources(dev);

	/* free wq, pi_dma_addr */
	hinic_free_all_rq(nic_dev);

	/* free wq, db_addr */
	hinic_free_all_sq(nic_dev);

	/* deinit mac vlan tbl */
	hinic_deinit_mac_addr(dev);
	hinic_remove_all_vlanid(dev);

	/* disable hardware and uio interrupt */
	hinic_disable_interrupt(dev);

	/* destroy rx mode mutex */
	hinic_mutex_destroy(&nic_dev->rx_mode_mutex);

	/* deinit nic hardware device */
	hinic_nic_dev_destroy(dev);

	return ret;
}

static const struct eth_dev_ops hinic_pmd_ops = {
	.dev_configure                 = hinic_dev_configure,
	.dev_infos_get                 = hinic_dev_infos_get,
	.fw_version_get                = hinic_fw_version_get,
	.rx_queue_setup                = hinic_rx_queue_setup,
	.tx_queue_setup                = hinic_tx_queue_setup,
	.dev_start                     = hinic_dev_start,
	.dev_set_link_up               = hinic_dev_set_link_up,
	.dev_set_link_down             = hinic_dev_set_link_down,
	.link_update                   = hinic_link_update,
	.rx_queue_release              = hinic_rx_queue_release,
	.tx_queue_release              = hinic_tx_queue_release,
	.dev_stop                      = hinic_dev_stop,
	.dev_close                     = hinic_dev_close,
	.mtu_set                       = hinic_dev_set_mtu,
	.vlan_filter_set               = hinic_vlan_filter_set,
	.vlan_offload_set              = hinic_vlan_offload_set,
	.allmulticast_enable           = hinic_dev_allmulticast_enable,
	.allmulticast_disable          = hinic_dev_allmulticast_disable,
	.promiscuous_enable            = hinic_dev_promiscuous_enable,
	.promiscuous_disable           = hinic_dev_promiscuous_disable,
	.flow_ctrl_get                 = hinic_flow_ctrl_get,
	.flow_ctrl_set                 = hinic_flow_ctrl_set,
	.rss_hash_update               = hinic_rss_hash_update,
	.rss_hash_conf_get             = hinic_rss_conf_get,
	.reta_update                   = hinic_rss_indirtbl_update,
	.reta_query                    = hinic_rss_indirtbl_query,
	.stats_get                     = hinic_dev_stats_get,
	.stats_reset                   = hinic_dev_stats_reset,
	.xstats_get                    = hinic_dev_xstats_get,
	.xstats_reset                  = hinic_dev_xstats_reset,
	.xstats_get_names              = hinic_dev_xstats_get_names,
	.rxq_info_get                  = hinic_rxq_info_get,
	.txq_info_get                  = hinic_txq_info_get,
	.mac_addr_set                  = hinic_set_mac_addr,
	.mac_addr_remove               = hinic_mac_addr_remove,
	.mac_addr_add                  = hinic_mac_addr_add,
	.set_mc_addr_list              = hinic_set_mc_addr_list,
	.flow_ops_get                  = hinic_dev_flow_ops_get,
};

static const struct eth_dev_ops hinic_pmd_vf_ops = {
	.dev_configure                 = hinic_dev_configure,
	.dev_infos_get                 = hinic_dev_infos_get,
	.fw_version_get                = hinic_fw_version_get,
	.rx_queue_setup                = hinic_rx_queue_setup,
	.tx_queue_setup                = hinic_tx_queue_setup,
	.dev_start                     = hinic_dev_start,
	.link_update                   = hinic_link_update,
	.rx_queue_release              = hinic_rx_queue_release,
	.tx_queue_release              = hinic_tx_queue_release,
	.dev_stop                      = hinic_dev_stop,
	.dev_close                     = hinic_dev_close,
	.mtu_set                       = hinic_dev_set_mtu,
	.vlan_filter_set               = hinic_vlan_filter_set,
	.vlan_offload_set              = hinic_vlan_offload_set,
	.allmulticast_enable           = hinic_dev_allmulticast_enable,
	.allmulticast_disable          = hinic_dev_allmulticast_disable,
	.rss_hash_update               = hinic_rss_hash_update,
	.rss_hash_conf_get             = hinic_rss_conf_get,
	.reta_update                   = hinic_rss_indirtbl_update,
	.reta_query                    = hinic_rss_indirtbl_query,
	.stats_get                     = hinic_dev_stats_get,
	.stats_reset                   = hinic_dev_stats_reset,
	.xstats_get                    = hinic_dev_xstats_get,
	.xstats_reset                  = hinic_dev_xstats_reset,
	.xstats_get_names              = hinic_dev_xstats_get_names,
	.rxq_info_get                  = hinic_rxq_info_get,
	.txq_info_get                  = hinic_txq_info_get,
	.mac_addr_set                  = hinic_set_mac_addr,
	.mac_addr_remove               = hinic_mac_addr_remove,
	.mac_addr_add                  = hinic_mac_addr_add,
	.set_mc_addr_list              = hinic_set_mc_addr_list,
	.flow_ops_get                  = hinic_dev_flow_ops_get,
};

static const struct eth_dev_ops hinic_dev_sec_ops = {
	.dev_infos_get                 = hinic_dev_infos_get,
};

static int hinic_func_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct rte_ether_addr *eth_addr;
	struct hinic_nic_dev *nic_dev;
	struct hinic_filter_info *filter_info;
	struct hinic_tcam_info *tcam_info;
	u32 mac_size;
	int rc;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* EAL is SECONDARY and eth_dev is already created */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev->dev_ops = &hinic_dev_sec_ops;
		PMD_DRV_LOG(INFO, "Initialize %s in secondary process",
			    eth_dev->data->name);

		return 0;
	}

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	memset(nic_dev, 0, sizeof(*nic_dev));

	snprintf(nic_dev->proc_dev_name,
		 sizeof(nic_dev->proc_dev_name),
		 "hinic-" PCI_PRI_FMT,
		 pci_dev->addr.domain, pci_dev->addr.bus,
		 pci_dev->addr.devid, pci_dev->addr.function);

	/* alloc mac_addrs */
	mac_size = HINIC_MAX_UC_MAC_ADDRS * sizeof(struct rte_ether_addr);
	eth_addr = rte_zmalloc("hinic_mac", mac_size, 0);
	if (!eth_addr) {
		PMD_DRV_LOG(ERR, "Allocate ethernet addresses' memory failed, dev_name: %s",
			    eth_dev->data->name);
		rc = -ENOMEM;
		goto eth_addr_fail;
	}
	eth_dev->data->mac_addrs = eth_addr;

	mac_size = HINIC_MAX_MC_MAC_ADDRS * sizeof(struct rte_ether_addr);
	nic_dev->mc_list = rte_zmalloc("hinic_mc", mac_size, 0);
	if (!nic_dev->mc_list) {
		PMD_DRV_LOG(ERR, "Allocate mcast address' memory failed, dev_name: %s",
			    eth_dev->data->name);
		rc = -ENOMEM;
		goto mc_addr_fail;
	}

	/* create hardware nic_device */
	rc = hinic_nic_dev_create(eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Create nic device failed, dev_name: %s",
			    eth_dev->data->name);
		goto create_nic_dev_fail;
	}

	if (HINIC_IS_VF(nic_dev->hwdev))
		eth_dev->dev_ops = &hinic_pmd_vf_ops;
	else
		eth_dev->dev_ops = &hinic_pmd_ops;

	rc = hinic_init_mac_addr(eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize mac table failed, dev_name: %s",
			    eth_dev->data->name);
		goto init_mac_fail;
	}

	/* register callback func to eal lib */
	rc = rte_intr_callback_register(pci_dev->intr_handle,
					hinic_dev_interrupt_handler,
					(void *)eth_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Register rte interrupt callback failed, dev_name: %s",
			    eth_dev->data->name);
		goto reg_intr_cb_fail;
	}

	/* enable uio/vfio intr/eventfd mapping */
	rc = rte_intr_enable(pci_dev->intr_handle);
	if (rc) {
		PMD_DRV_LOG(ERR, "Enable rte interrupt failed, dev_name: %s",
			    eth_dev->data->name);
		goto enable_intr_fail;
	}
	rte_bit_relaxed_set32(HINIC_DEV_INTR_EN, &nic_dev->dev_status);

	hinic_mutex_init(&nic_dev->rx_mode_mutex, NULL);

	/* initialize filter info */
	filter_info = &nic_dev->filter;
	tcam_info = &nic_dev->tcam;
	memset(filter_info, 0, sizeof(struct hinic_filter_info));
	memset(tcam_info, 0, sizeof(struct hinic_tcam_info));
	/* initialize 5tuple filter list */
	TAILQ_INIT(&filter_info->fivetuple_list);
	TAILQ_INIT(&tcam_info->tcam_list);
	TAILQ_INIT(&nic_dev->filter_ntuple_list);
	TAILQ_INIT(&nic_dev->filter_ethertype_list);
	TAILQ_INIT(&nic_dev->filter_fdir_rule_list);
	TAILQ_INIT(&nic_dev->hinic_flow_list);

	rte_bit_relaxed_set32(HINIC_DEV_INIT, &nic_dev->dev_status);
	PMD_DRV_LOG(INFO, "Initialize %s in primary successfully",
		    eth_dev->data->name);

	return 0;

enable_intr_fail:
	(void)rte_intr_callback_unregister(pci_dev->intr_handle,
					   hinic_dev_interrupt_handler,
					   (void *)eth_dev);

reg_intr_cb_fail:
	hinic_deinit_mac_addr(eth_dev);

init_mac_fail:
	eth_dev->dev_ops = NULL;
	hinic_nic_dev_destroy(eth_dev);

create_nic_dev_fail:
	rte_free(nic_dev->mc_list);
	nic_dev->mc_list = NULL;

mc_addr_fail:
	rte_free(eth_addr);
	eth_dev->data->mac_addrs = NULL;

eth_addr_fail:
	PMD_DRV_LOG(ERR, "Initialize %s in primary failed",
		    eth_dev->data->name);
	return rc;
}

static int hinic_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_DRV_LOG(INFO, "Initializing pf hinic-" PCI_PRI_FMT " in %s process",
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function,
		    (rte_eal_process_type() == RTE_PROC_PRIMARY) ?
		    "primary" : "secondary");

	/* rte_eth_dev rx_burst and tx_burst */
	eth_dev->rx_pkt_burst = hinic_recv_pkts;
	eth_dev->tx_pkt_burst = hinic_xmit_pkts;

	return hinic_func_init(eth_dev);
}

static int hinic_dev_uninit(struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hinic_dev_close(dev);

	return HINIC_OK;
}

static struct rte_pci_id pci_id_hinic_map[] = {
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_PRD) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_25GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_100GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_VF) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_VF_HV) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_1822_DUAL_25GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_1822_100GE) },
	{.vendor_id = 0},
};

static int hinic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct hinic_nic_dev), hinic_dev_init);
}

static int hinic_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hinic_dev_uninit);
}

static struct rte_pci_driver rte_hinic_pmd = {
	.id_table = pci_id_hinic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = hinic_pci_probe,
	.remove = hinic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hinic, rte_hinic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hinic, pci_id_hinic_map);
RTE_LOG_REGISTER_DEFAULT(hinic_logtype, INFO);
