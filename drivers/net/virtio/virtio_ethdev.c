/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <ethdev_driver.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_arp.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_cpuflags.h>
#include <rte_vect.h>
#include <rte_memory.h>
#include <rte_eal_paging.h>
#include <rte_eal.h>
#include <dev_driver.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>

#include "virtio_ethdev.h"
#include "virtio.h"
#include "virtio_logs.h"
#include "virtqueue.h"
#include "virtio_cvq.h"
#include "virtio_rxtx.h"
#include "virtio_rxtx_simple.h"
#include "virtio_user/virtio_user_dev.h"

static int  virtio_dev_configure(struct rte_eth_dev *dev);
static int  virtio_dev_start(struct rte_eth_dev *dev);
static int virtio_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int virtio_dev_promiscuous_disable(struct rte_eth_dev *dev);
static int virtio_dev_allmulticast_enable(struct rte_eth_dev *dev);
static int virtio_dev_allmulticast_disable(struct rte_eth_dev *dev);
static uint32_t virtio_dev_speed_capa_get(uint32_t speed);
static int virtio_dev_devargs_parse(struct rte_devargs *devargs,
	uint32_t *speed,
	int *vectorized);
static int virtio_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int virtio_dev_link_update(struct rte_eth_dev *dev,
	int wait_to_complete);
static int virtio_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int virtio_dev_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf);
static int virtio_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf);
static int virtio_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size);
static int virtio_dev_rss_reta_query(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size);

static void virtio_set_hwaddr(struct virtio_hw *hw);
static void virtio_get_hwaddr(struct virtio_hw *hw);

static int virtio_dev_stats_get(struct rte_eth_dev *dev,
				 struct rte_eth_stats *stats);
static int virtio_dev_xstats_get(struct rte_eth_dev *dev,
				 struct rte_eth_xstat *xstats, unsigned n);
static int virtio_dev_xstats_get_names(struct rte_eth_dev *dev,
				       struct rte_eth_xstat_name *xstats_names,
				       unsigned limit);
static int virtio_dev_stats_reset(struct rte_eth_dev *dev);
static void virtio_dev_free_mbufs(struct rte_eth_dev *dev);
static int virtio_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id, int on);
static int virtio_mac_addr_add(struct rte_eth_dev *dev,
				struct rte_ether_addr *mac_addr,
				uint32_t index, uint32_t vmdq);
static void virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
static int virtio_mac_addr_set(struct rte_eth_dev *dev,
				struct rte_ether_addr *mac_addr);

static int virtio_intr_disable(struct rte_eth_dev *dev);
static int virtio_get_monitor_addr(void *rx_queue,
				struct rte_power_monitor_cond *pmc);

static int virtio_dev_queue_stats_mapping_set(
	struct rte_eth_dev *eth_dev,
	uint16_t queue_id,
	uint8_t stat_idx,
	uint8_t is_rx);

static void virtio_notify_peers(struct rte_eth_dev *dev);
static void virtio_ack_link_announce(struct rte_eth_dev *dev);

struct rte_virtio_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

/* [rt]x_qX_ is prepended to the name string here */
static const struct rte_virtio_xstats_name_off rte_virtio_rxq_stat_strings[] = {
	{"good_packets",           offsetof(struct virtnet_rx, stats.packets)},
	{"good_bytes",             offsetof(struct virtnet_rx, stats.bytes)},
	{"errors",                 offsetof(struct virtnet_rx, stats.errors)},
	{"multicast_packets",      offsetof(struct virtnet_rx, stats.multicast)},
	{"broadcast_packets",      offsetof(struct virtnet_rx, stats.broadcast)},
	{"undersize_packets",      offsetof(struct virtnet_rx, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct virtnet_rx, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct virtnet_rx, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct virtnet_rx, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct virtnet_rx, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct virtnet_rx, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct virtnet_rx, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct virtnet_rx, stats.size_bins[7])},
};

/* [rt]x_qX_ is prepended to the name string here */
static const struct rte_virtio_xstats_name_off rte_virtio_txq_stat_strings[] = {
	{"good_packets",           offsetof(struct virtnet_tx, stats.packets)},
	{"good_bytes",             offsetof(struct virtnet_tx, stats.bytes)},
	{"multicast_packets",      offsetof(struct virtnet_tx, stats.multicast)},
	{"broadcast_packets",      offsetof(struct virtnet_tx, stats.broadcast)},
	{"undersize_packets",      offsetof(struct virtnet_tx, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct virtnet_tx, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct virtnet_tx, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct virtnet_tx, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct virtnet_tx, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct virtnet_tx, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct virtnet_tx, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct virtnet_tx, stats.size_bins[7])},
};

#define VIRTIO_NB_RXQ_XSTATS (sizeof(rte_virtio_rxq_stat_strings) / \
			    sizeof(rte_virtio_rxq_stat_strings[0]))
#define VIRTIO_NB_TXQ_XSTATS (sizeof(rte_virtio_txq_stat_strings) / \
			    sizeof(rte_virtio_txq_stat_strings[0]))

struct virtio_hw_internal virtio_hw_internal[RTE_MAX_ETHPORTS];

static int
virtio_set_multiple_queues_rss(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	struct virtio_net_ctrl_rss rss;
	int dlen, ret;

	rss.hash_types = hw->rss_hash_types & VIRTIO_NET_HASH_TYPE_MASK;
	RTE_BUILD_BUG_ON(!RTE_IS_POWER_OF_2(VIRTIO_NET_RSS_RETA_SIZE));
	rss.indirection_table_mask = VIRTIO_NET_RSS_RETA_SIZE - 1;
	rss.unclassified_queue = 0;
	memcpy(rss.indirection_table, hw->rss_reta, VIRTIO_NET_RSS_RETA_SIZE * sizeof(uint16_t));
	rss.max_tx_vq = nb_queues;
	rss.hash_key_length = VIRTIO_NET_RSS_KEY_SIZE;
	memcpy(rss.hash_key_data, hw->rss_key, VIRTIO_NET_RSS_KEY_SIZE);

	ctrl.hdr.class = VIRTIO_NET_CTRL_MQ;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_MQ_RSS_CONFIG;
	memcpy(ctrl.data, &rss, sizeof(rss));

	dlen = sizeof(rss);

	ret = virtio_send_command(hw->cvq, &ctrl, &dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "RSS multiqueue configured but send command failed");
		return -EINVAL;
	}

	return 0;
}

static int
virtio_set_multiple_queues_auto(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen;
	int ret;

	ctrl.hdr.class = VIRTIO_NET_CTRL_MQ;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
	memcpy(ctrl.data, &nb_queues, sizeof(uint16_t));

	dlen = sizeof(uint16_t);

	ret = virtio_send_command(hw->cvq, &ctrl, &dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Multiqueue configured but send command "
			  "failed, this is too late now...");
		return -EINVAL;
	}

	return 0;
}

static int
virtio_set_multiple_queues(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (virtio_with_feature(hw, VIRTIO_NET_F_RSS))
		return virtio_set_multiple_queues_rss(dev, nb_queues);
	else
		return virtio_set_multiple_queues_auto(dev, nb_queues);
}

static uint16_t
virtio_get_nr_vq(struct virtio_hw *hw)
{
	uint16_t nr_vq = hw->max_queue_pairs * 2;

	if (virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VQ))
		nr_vq += 1;

	return nr_vq;
}

static void
virtio_control_queue_notify(struct virtqueue *vq, __rte_unused void *cookie)
{
	virtqueue_notify(vq);
}

static int
virtio_init_queue(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	unsigned int vq_size;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq;
	int queue_type = virtio_get_queue_type(hw, queue_idx);
	int ret;
	int numa_node = dev->device->numa_node;

	PMD_INIT_LOG(INFO, "setting up queue: %u on NUMA node %d",
			queue_idx, numa_node);

	/*
	 * Read the virtqueue size from the Queue Size field
	 * Always power of 2 and if 0 virtqueue does not exist
	 */
	vq_size = VIRTIO_OPS(hw)->get_queue_num(hw, queue_idx);
	PMD_INIT_LOG(DEBUG, "vq_size: %u", vq_size);
	if (vq_size == 0) {
		PMD_INIT_LOG(ERR, "virtqueue does not exist");
		return -EINVAL;
	}

	if (!virtio_with_packed_queue(hw) && !rte_is_power_of_2(vq_size)) {
		PMD_INIT_LOG(ERR, "split virtqueue size is not power of 2");
		return -EINVAL;
	}

	snprintf(vq_name, sizeof(vq_name), "port%d_vq%d", dev->data->port_id, queue_idx);

	vq = virtqueue_alloc(hw, queue_idx, vq_size, queue_type, numa_node, vq_name);
	if (!vq) {
		PMD_INIT_LOG(ERR, "virtqueue init failed");
		return -ENOMEM;
	}

	hw->vqs[queue_idx] = vq;

	if (queue_type == VTNET_CQ) {
		hw->cvq = &vq->cq;
		vq->cq.notify_queue = &virtio_control_queue_notify;
	}

	if (VIRTIO_OPS(hw)->setup_queue(hw, vq) < 0) {
		PMD_INIT_LOG(ERR, "setup_queue failed");
		ret = -EINVAL;
		goto clean_vq;
	}

	return 0;

clean_vq:
	if (queue_type == VTNET_CQ)
		hw->cvq = NULL;
	virtqueue_free(vq);
	hw->vqs[queue_idx] = NULL;

	return ret;
}

static void
virtio_free_queues(struct virtio_hw *hw)
{
	uint16_t nr_vq = virtio_get_nr_vq(hw);
	struct virtqueue *vq;
	uint16_t i;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (!vq)
			continue;
		virtqueue_free(vq);
		hw->vqs[i] = NULL;
	}

	rte_free(hw->vqs);
	hw->vqs = NULL;
}

static int
virtio_alloc_queues(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = virtio_get_nr_vq(hw);
	uint16_t i;
	int ret;

	hw->vqs = rte_zmalloc(NULL, sizeof(struct virtqueue *) * nr_vq, 0);
	if (!hw->vqs) {
		PMD_INIT_LOG(ERR, "failed to allocate vqs");
		return -ENOMEM;
	}

	for (i = 0; i < nr_vq; i++) {
		ret = virtio_init_queue(dev, i);
		if (ret < 0) {
			virtio_free_queues(hw);
			return ret;
		}
	}

	return 0;
}

static void virtio_queues_unbind_intr(struct rte_eth_dev *dev);

static void
virtio_free_rss(struct virtio_hw *hw)
{
	rte_free(hw->rss_key);
	hw->rss_key = NULL;

	rte_free(hw->rss_reta);
	hw->rss_reta = NULL;
}

int
virtio_dev_close(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_eth_intr_conf *intr_conf = &dev->data->dev_conf.intr_conf;

	PMD_INIT_LOG(DEBUG, "virtio_dev_close");
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!hw->opened)
		return 0;
	hw->opened = 0;

	/* reset the NIC */
	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		VIRTIO_OPS(hw)->set_config_irq(hw, VIRTIO_MSI_NO_VECTOR);
	if (intr_conf->rxq)
		virtio_queues_unbind_intr(dev);

	if (intr_conf->lsc || intr_conf->rxq) {
		virtio_intr_disable(dev);
		rte_intr_efd_disable(dev->intr_handle);
		rte_intr_vec_list_free(dev->intr_handle);
	}

	virtio_reset(hw);
	virtio_dev_free_mbufs(dev);
	virtio_free_queues(hw);
	virtio_free_rss(hw);

	return VIRTIO_OPS(hw)->dev_close(hw);
}

static int
virtio_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_PROMISC;
	ctrl.data[0] = 1;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to enable promisc");
		return -EAGAIN;
	}

	return 0;
}

static int
virtio_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_PROMISC;
	ctrl.data[0] = 0;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to disable promisc");
		return -EAGAIN;
	}

	return 0;
}

static int
virtio_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_ALLMULTI;
	ctrl.data[0] = 1;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to enable allmulticast");
		return -EAGAIN;
	}

	return 0;
}

static int
virtio_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int dlen[1];
	int ret;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_RX)) {
		PMD_INIT_LOG(INFO, "host does not support rx control");
		return -ENOTSUP;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_RX;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_RX_ALLMULTI;
	ctrl.data[0] = 0;
	dlen[0] = 1;

	ret = virtio_send_command(hw->cvq, &ctrl, dlen, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to disable allmulticast");
		return -EAGAIN;
	}

	return 0;
}

uint16_t
virtio_rx_mem_pool_buf_size(struct rte_mempool *mp)
{
	return rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM;
}

bool
virtio_rx_check_scatter(uint16_t max_rx_pkt_len, uint16_t rx_buf_size,
			bool rx_scatter_enabled, const char **error)
{
	if (!rx_scatter_enabled && max_rx_pkt_len > rx_buf_size) {
		*error = "Rx scatter is disabled and RxQ mbuf pool object size is too small";
		return false;
	}

	return true;
}

static bool
virtio_check_scatter_on_all_rx_queues(struct rte_eth_dev *dev,
				      uint16_t frame_size)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_rx *rxvq;
	struct virtqueue *vq;
	unsigned int qidx;
	uint16_t buf_size;
	const char *error;

	if (hw->vqs == NULL)
		return true;

	for (qidx = 0; qidx < hw->max_queue_pairs; qidx++) {
		vq = hw->vqs[2 * qidx + VTNET_SQ_RQ_QUEUE_IDX];
		if (vq == NULL)
			continue;

		rxvq = &vq->rxq;
		if (rxvq->mpool == NULL)
			continue;
		buf_size = virtio_rx_mem_pool_buf_size(rxvq->mpool);

		if (!virtio_rx_check_scatter(frame_size, buf_size,
					     hw->rx_ol_scatter, &error)) {
			PMD_INIT_LOG(ERR, "MTU check for RxQ %u failed: %s",
				     qidx, error);
			return false;
		}
	}

	return true;
}

#define VLAN_TAG_LEN           4    /* 802.3ac tag (not DMA'd) */
static int
virtio_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint32_t ether_hdr_len = RTE_ETHER_HDR_LEN + VLAN_TAG_LEN +
				 hw->vtnet_hdr_size;
	uint32_t frame_size = mtu + ether_hdr_len;
	uint32_t max_frame_size = hw->max_mtu + ether_hdr_len;

	max_frame_size = RTE_MIN(max_frame_size, VIRTIO_MAX_RX_PKTLEN);

	if (mtu < RTE_ETHER_MIN_MTU || frame_size > max_frame_size) {
		PMD_INIT_LOG(ERR, "MTU should be between %d and %d",
			RTE_ETHER_MIN_MTU, max_frame_size - ether_hdr_len);
		return -EINVAL;
	}

	if (!virtio_check_scatter_on_all_rx_queues(dev, frame_size)) {
		PMD_INIT_LOG(ERR, "MTU vs Rx scatter and Rx buffers check failed");
		return -EINVAL;
	}

	hw->max_rx_pkt_len = frame_size;

	return 0;
}

static int
virtio_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);

	virtqueue_enable_intr(vq);
	virtio_mb(hw->weak_barriers);
	return 0;
}

static int
virtio_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);

	virtqueue_disable_intr(vq);
	return 0;
}

static int
virtio_dev_priv_dump(struct rte_eth_dev *dev, FILE *f)
{
	struct virtio_hw *hw = dev->data->dev_private;

	fprintf(f, "guest_features: 0x%" PRIx64 "\n", hw->guest_features);
	fprintf(f, "vtnet_hdr_size: %u\n", hw->vtnet_hdr_size);
	fprintf(f, "use_vec: rx-%u tx-%u\n", hw->use_vec_rx, hw->use_vec_tx);
	fprintf(f, "use_inorder: rx-%u tx-%u\n", hw->use_inorder_rx, hw->use_inorder_tx);
	fprintf(f, "intr_lsc: %u\n", hw->intr_lsc);
	fprintf(f, "max_mtu: %u\n", hw->max_mtu);
	fprintf(f, "max_rx_pkt_len: %zu\n", hw->max_rx_pkt_len);
	fprintf(f, "max_queue_pairs: %u\n", hw->max_queue_pairs);
	fprintf(f, "req_guest_features: 0x%" PRIx64 "\n", hw->req_guest_features);

	return 0;
}

/*
 * dev_ops for virtio, bare necessities for basic operation
 */
static const struct eth_dev_ops virtio_eth_dev_ops = {
	.dev_configure           = virtio_dev_configure,
	.dev_start               = virtio_dev_start,
	.dev_stop                = virtio_dev_stop,
	.dev_close               = virtio_dev_close,
	.promiscuous_enable      = virtio_dev_promiscuous_enable,
	.promiscuous_disable     = virtio_dev_promiscuous_disable,
	.allmulticast_enable     = virtio_dev_allmulticast_enable,
	.allmulticast_disable    = virtio_dev_allmulticast_disable,
	.mtu_set                 = virtio_mtu_set,
	.dev_infos_get           = virtio_dev_info_get,
	.stats_get               = virtio_dev_stats_get,
	.xstats_get              = virtio_dev_xstats_get,
	.xstats_get_names        = virtio_dev_xstats_get_names,
	.stats_reset             = virtio_dev_stats_reset,
	.xstats_reset            = virtio_dev_stats_reset,
	.link_update             = virtio_dev_link_update,
	.vlan_offload_set        = virtio_dev_vlan_offload_set,
	.rx_queue_setup          = virtio_dev_rx_queue_setup,
	.rx_queue_intr_enable    = virtio_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable   = virtio_dev_rx_queue_intr_disable,
	.tx_queue_setup          = virtio_dev_tx_queue_setup,
	.rss_hash_update         = virtio_dev_rss_hash_update,
	.rss_hash_conf_get       = virtio_dev_rss_hash_conf_get,
	.reta_update             = virtio_dev_rss_reta_update,
	.reta_query              = virtio_dev_rss_reta_query,
	/* collect stats per queue */
	.queue_stats_mapping_set = virtio_dev_queue_stats_mapping_set,
	.vlan_filter_set         = virtio_vlan_filter_set,
	.mac_addr_add            = virtio_mac_addr_add,
	.mac_addr_remove         = virtio_mac_addr_remove,
	.mac_addr_set            = virtio_mac_addr_set,
	.get_monitor_addr        = virtio_get_monitor_addr,
	.eth_dev_priv_dump       = virtio_dev_priv_dump,
};

/*
 * dev_ops for virtio-user in secondary processes, as we just have
 * some limited supports currently.
 */
const struct eth_dev_ops virtio_user_secondary_eth_dev_ops = {
	.dev_infos_get           = virtio_dev_info_get,
	.stats_get               = virtio_dev_stats_get,
	.xstats_get              = virtio_dev_xstats_get,
	.xstats_get_names        = virtio_dev_xstats_get_names,
	.stats_reset             = virtio_dev_stats_reset,
	.xstats_reset            = virtio_dev_stats_reset,
	/* collect stats per queue */
	.queue_stats_mapping_set = virtio_dev_queue_stats_mapping_set,
};

static void
virtio_update_stats(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		const struct virtnet_tx *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		stats->opackets += txvq->stats.packets;
		stats->obytes += txvq->stats.bytes;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txvq->stats.packets;
			stats->q_obytes[i] = txvq->stats.bytes;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		const struct virtnet_rx *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;

		stats->ipackets += rxvq->stats.packets;
		stats->ibytes += rxvq->stats.bytes;
		stats->ierrors += rxvq->stats.errors;

		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxvq->stats.packets;
			stats->q_ibytes[i] = rxvq->stats.bytes;
		}
	}

	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
}

static int virtio_dev_xstats_get_names(struct rte_eth_dev *dev,
				       struct rte_eth_xstat_name *xstats_names,
				       __rte_unused unsigned limit)
{
	unsigned i;
	unsigned count = 0;
	unsigned t;

	unsigned nstats = dev->data->nb_tx_queues * VIRTIO_NB_TXQ_XSTATS +
		dev->data->nb_rx_queues * VIRTIO_NB_RXQ_XSTATS;

	if (xstats_names != NULL) {
		/* Note: limit checked in rte_eth_xstats_names() */

		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			struct virtnet_rx *rxvq = dev->data->rx_queues[i];
			if (rxvq == NULL)
				continue;
			for (t = 0; t < VIRTIO_NB_RXQ_XSTATS; t++) {
				snprintf(xstats_names[count].name,
					sizeof(xstats_names[count].name),
					"rx_q%u_%s", i,
					rte_virtio_rxq_stat_strings[t].name);
				count++;
			}
		}

		for (i = 0; i < dev->data->nb_tx_queues; i++) {
			struct virtnet_tx *txvq = dev->data->tx_queues[i];
			if (txvq == NULL)
				continue;
			for (t = 0; t < VIRTIO_NB_TXQ_XSTATS; t++) {
				snprintf(xstats_names[count].name,
					sizeof(xstats_names[count].name),
					"tx_q%u_%s", i,
					rte_virtio_txq_stat_strings[t].name);
				count++;
			}
		}
		return count;
	}
	return nstats;
}

static int
virtio_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		      unsigned n)
{
	unsigned i;
	unsigned count = 0;

	unsigned nstats = dev->data->nb_tx_queues * VIRTIO_NB_TXQ_XSTATS +
		dev->data->nb_rx_queues * VIRTIO_NB_RXQ_XSTATS;

	if (n < nstats)
		return nstats;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtnet_rx *rxvq = dev->data->rx_queues[i];

		if (rxvq == NULL)
			continue;

		unsigned t;

		for (t = 0; t < VIRTIO_NB_RXQ_XSTATS; t++) {
			xstats[count].value = *(uint64_t *)(((char *)rxvq) +
				rte_virtio_rxq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];

		if (txvq == NULL)
			continue;

		unsigned t;

		for (t = 0; t < VIRTIO_NB_TXQ_XSTATS; t++) {
			xstats[count].value = *(uint64_t *)(((char *)txvq) +
				rte_virtio_txq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}

	return count;
}

static int
virtio_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	virtio_update_stats(dev, stats);

	return 0;
}

static int
virtio_dev_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;

		txvq->stats.packets = 0;
		txvq->stats.bytes = 0;
		txvq->stats.multicast = 0;
		txvq->stats.broadcast = 0;
		memset(txvq->stats.size_bins, 0,
		       sizeof(txvq->stats.size_bins[0]) * 8);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtnet_rx *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;

		rxvq->stats.packets = 0;
		rxvq->stats.bytes = 0;
		rxvq->stats.errors = 0;
		rxvq->stats.multicast = 0;
		rxvq->stats.broadcast = 0;
		memset(rxvq->stats.size_bins, 0,
		       sizeof(rxvq->stats.size_bins[0]) * 8);
	}

	return 0;
}

static void
virtio_set_hwaddr(struct virtio_hw *hw)
{
	virtio_write_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&hw->mac_addr, RTE_ETHER_ADDR_LEN);
}

static void
virtio_get_hwaddr(struct virtio_hw *hw)
{
	if (virtio_with_feature(hw, VIRTIO_NET_F_MAC)) {
		virtio_read_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&hw->mac_addr, RTE_ETHER_ADDR_LEN);
	} else {
		rte_eth_random_addr(&hw->mac_addr[0]);
		virtio_set_hwaddr(hw);
	}
}

static int
virtio_mac_table_set(struct virtio_hw *hw,
		     const struct virtio_net_ctrl_mac *uc,
		     const struct virtio_net_ctrl_mac *mc)
{
	struct virtio_pmd_ctrl ctrl;
	int err, len[2];

	if (!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_MAC_ADDR)) {
		PMD_DRV_LOG(INFO, "host does not support mac table");
		return -1;
	}

	ctrl.hdr.class = VIRTIO_NET_CTRL_MAC;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_MAC_TABLE_SET;

	len[0] = uc->entries * RTE_ETHER_ADDR_LEN + sizeof(uc->entries);
	memcpy(ctrl.data, uc, len[0]);

	len[1] = mc->entries * RTE_ETHER_ADDR_LEN + sizeof(mc->entries);
	memcpy(ctrl.data + len[0], mc, len[1]);

	err = virtio_send_command(hw->cvq, &ctrl, len, 2);
	if (err != 0)
		PMD_DRV_LOG(NOTICE, "mac table set failed: %d", err);
	return err;
}

static int
virtio_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		    uint32_t index, uint32_t vmdq __rte_unused)
{
	struct virtio_hw *hw = dev->data->dev_private;
	const struct rte_ether_addr *addrs = dev->data->mac_addrs;
	unsigned int i;
	struct virtio_net_ctrl_mac *uc, *mc;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return -EINVAL;
	}

	uc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(uc->entries));
	uc->entries = 0;
	mc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(mc->entries));
	mc->entries = 0;

	for (i = 0; i < VIRTIO_MAX_MAC_ADDRS; i++) {
		const struct rte_ether_addr *addr
			= (i == index) ? mac_addr : addrs + i;
		struct virtio_net_ctrl_mac *tbl
			= rte_is_multicast_ether_addr(addr) ? mc : uc;

		memcpy(&tbl->macs[tbl->entries++], addr, RTE_ETHER_ADDR_LEN);
	}

	return virtio_mac_table_set(hw, uc, mc);
}

static void
virtio_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_ether_addr *addrs = dev->data->mac_addrs;
	struct virtio_net_ctrl_mac *uc, *mc;
	unsigned int i;

	if (index >= VIRTIO_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "mac address index %u out of range", index);
		return;
	}

	uc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(uc->entries));
	uc->entries = 0;
	mc = alloca(VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN +
		sizeof(mc->entries));
	mc->entries = 0;

	for (i = 0; i < VIRTIO_MAX_MAC_ADDRS; i++) {
		struct virtio_net_ctrl_mac *tbl;

		if (i == index || rte_is_zero_ether_addr(addrs + i))
			continue;

		tbl = rte_is_multicast_ether_addr(addrs + i) ? mc : uc;
		memcpy(&tbl->macs[tbl->entries++], addrs + i,
			RTE_ETHER_ADDR_LEN);
	}

	virtio_mac_table_set(hw, uc, mc);
}

static int
virtio_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct virtio_hw *hw = dev->data->dev_private;

	memcpy(hw->mac_addr, mac_addr, RTE_ETHER_ADDR_LEN);

	/* Use atomic update if available */
	if (virtio_with_feature(hw, VIRTIO_NET_F_CTRL_MAC_ADDR)) {
		struct virtio_pmd_ctrl ctrl;
		int len = RTE_ETHER_ADDR_LEN;

		ctrl.hdr.class = VIRTIO_NET_CTRL_MAC;
		ctrl.hdr.cmd = VIRTIO_NET_CTRL_MAC_ADDR_SET;

		memcpy(ctrl.data, mac_addr, RTE_ETHER_ADDR_LEN);
		return virtio_send_command(hw->cvq, &ctrl, &len, 1);
	}

	if (!virtio_with_feature(hw, VIRTIO_NET_F_MAC))
		return -ENOTSUP;

	virtio_set_hwaddr(hw);
	return 0;
}

#define CLB_VAL_IDX 0
#define CLB_MSK_IDX 1
#define CLB_MATCH_IDX 2
static int
virtio_monitor_callback(const uint64_t value,
		const uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ])
{
	const uint64_t m = opaque[CLB_MSK_IDX];
	const uint64_t v = opaque[CLB_VAL_IDX];
	const uint64_t c = opaque[CLB_MATCH_IDX];

	if (c)
		return (value & m) == v ? -1 : 0;
	else
		return (value & m) == v ? 0 : -1;
}

static int
virtio_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw;

	if (vq == NULL)
		return -EINVAL;

	hw = vq->hw;
	if (virtio_with_packed_queue(hw)) {
		struct vring_packed_desc *desc;
		desc = vq->vq_packed.ring.desc;
		pmc->addr = &desc[vq->vq_used_cons_idx].flags;
		if (vq->vq_packed.used_wrap_counter)
			pmc->opaque[CLB_VAL_IDX] =
						VRING_PACKED_DESC_F_AVAIL_USED;
		else
			pmc->opaque[CLB_VAL_IDX] = 0;
		pmc->opaque[CLB_MSK_IDX] = VRING_PACKED_DESC_F_AVAIL_USED;
		pmc->opaque[CLB_MATCH_IDX] = 1;
		pmc->size = sizeof(desc[vq->vq_used_cons_idx].flags);
	} else {
		pmc->addr = &vq->vq_split.ring.used->idx;
		pmc->opaque[CLB_VAL_IDX] = vq->vq_used_cons_idx
					& (vq->vq_nentries - 1);
		pmc->opaque[CLB_MSK_IDX] = vq->vq_nentries - 1;
		pmc->opaque[CLB_MATCH_IDX] = 0;
		pmc->size = sizeof(vq->vq_split.ring.used->idx);
	}
	pmc->fn = virtio_monitor_callback;

	return 0;
}

static int
virtio_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;
	int len;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN))
		return -ENOTSUP;

	ctrl.hdr.class = VIRTIO_NET_CTRL_VLAN;
	ctrl.hdr.cmd = on ? VIRTIO_NET_CTRL_VLAN_ADD : VIRTIO_NET_CTRL_VLAN_DEL;
	memcpy(ctrl.data, &vlan_id, sizeof(vlan_id));
	len = sizeof(vlan_id);

	return virtio_send_command(hw->cvq, &ctrl, &len, 1);
}

static int
virtio_intr_unmask(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (rte_intr_ack(dev->intr_handle) < 0)
		return -1;

	if (VIRTIO_OPS(hw)->intr_detect)
		VIRTIO_OPS(hw)->intr_detect(hw);

	return 0;
}

static int
virtio_intr_enable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (rte_intr_enable(dev->intr_handle) < 0)
		return -1;

	if (VIRTIO_OPS(hw)->intr_detect)
		VIRTIO_OPS(hw)->intr_detect(hw);

	return 0;
}

static int
virtio_intr_disable(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (rte_intr_disable(dev->intr_handle) < 0)
		return -1;

	if (VIRTIO_OPS(hw)->intr_detect)
		VIRTIO_OPS(hw)->intr_detect(hw);

	return 0;
}

static int
virtio_ethdev_negotiate_features(struct virtio_hw *hw, uint64_t req_features)
{
	uint64_t host_features;

	/* Prepare guest_features: feature that driver wants to support */
	PMD_INIT_LOG(DEBUG, "guest_features before negotiate = %" PRIx64,
		req_features);

	/* Read device(host) feature bits */
	host_features = VIRTIO_OPS(hw)->get_features(hw);
	PMD_INIT_LOG(DEBUG, "host_features before negotiate = %" PRIx64,
		host_features);

	/* If supported, ensure MTU value is valid before acknowledging it. */
	if (host_features & req_features & (1ULL << VIRTIO_NET_F_MTU)) {
		struct virtio_net_config config;

		virtio_read_dev_config(hw,
			offsetof(struct virtio_net_config, mtu),
			&config.mtu, sizeof(config.mtu));

		if (config.mtu < RTE_ETHER_MIN_MTU)
			req_features &= ~(1ULL << VIRTIO_NET_F_MTU);
	}

	/*
	 * Negotiate features: Subset of device feature bits are written back
	 * guest feature bits.
	 */
	hw->guest_features = req_features;
	hw->guest_features = virtio_negotiate_features(hw, host_features);
	PMD_INIT_LOG(DEBUG, "features after negotiate = %" PRIx64,
		hw->guest_features);

	if (VIRTIO_OPS(hw)->features_ok(hw) < 0)
		return -1;

	if (virtio_with_feature(hw, VIRTIO_F_VERSION_1)) {
		virtio_set_status(hw, VIRTIO_CONFIG_STATUS_FEATURES_OK);

		if (!(virtio_get_status(hw) & VIRTIO_CONFIG_STATUS_FEATURES_OK)) {
			PMD_INIT_LOG(ERR, "Failed to set FEATURES_OK status!");
			return -1;
		}
	}

	hw->req_guest_features = req_features;

	return 0;
}

static void
virtio_notify_peers(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtnet_rx *rxvq;
	struct rte_mbuf *rarp_mbuf;

	if (!dev->data->rx_queues)
		return;

	rxvq = dev->data->rx_queues[0];
	if (!rxvq)
		return;

	rarp_mbuf = rte_net_make_rarp_packet(rxvq->mpool,
			(struct rte_ether_addr *)hw->mac_addr);
	if (rarp_mbuf == NULL) {
		PMD_DRV_LOG(ERR, "failed to make RARP packet.");
		return;
	}

	rte_spinlock_lock(&hw->state_lock);
	if (hw->started == 0) {
		/* If virtio port just stopped, no need to send RARP */
		rte_pktmbuf_free(rarp_mbuf);
		goto out;
	}
	hw->started = 0;

	/*
	 * Prevent the worker threads from touching queues to avoid contention,
	 * 1 ms should be enough for the ongoing Tx function to finish.
	 */
	rte_delay_ms(1);

	hw->inject_pkts = &rarp_mbuf;
	dev->tx_pkt_burst(dev->data->tx_queues[0], &rarp_mbuf, 1);
	hw->inject_pkts = NULL;

	hw->started = 1;

out:
	rte_spinlock_unlock(&hw->state_lock);
}

static void
virtio_ack_link_announce(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtio_pmd_ctrl ctrl;

	ctrl.hdr.class = VIRTIO_NET_CTRL_ANNOUNCE;
	ctrl.hdr.cmd = VIRTIO_NET_CTRL_ANNOUNCE_ACK;

	virtio_send_command(hw->cvq, &ctrl, NULL, 0);
}

/*
 * Process virtio config changed interrupt. Call the callback
 * if link state changed, generate gratuitous RARP packet if
 * the status indicates an ANNOUNCE.
 */
void
virtio_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct virtio_hw *hw = dev->data->dev_private;
	uint8_t isr;
	uint16_t status;

	/* Read interrupt status which clears interrupt */
	isr = virtio_get_isr(hw);
	PMD_DRV_LOG(INFO, "interrupt status = %#x", isr);

	if (virtio_intr_unmask(dev) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");

	if (isr & VIRTIO_ISR_CONFIG) {
		if (virtio_dev_link_update(dev, 0) == 0)
			rte_eth_dev_callback_process(dev,
						     RTE_ETH_EVENT_INTR_LSC,
						     NULL);

		if (virtio_with_feature(hw, VIRTIO_NET_F_STATUS)) {
			virtio_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&status, sizeof(status));
			if (status & VIRTIO_NET_S_ANNOUNCE) {
				virtio_notify_peers(dev);
				if (hw->cvq)
					virtio_ack_link_announce(dev);
			}
		}
	}
}

/* set rx and tx handlers according to what is supported */
static void
set_rxtx_funcs(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;

	eth_dev->tx_pkt_prepare = virtio_xmit_pkts_prepare;
	if (virtio_with_packed_queue(hw)) {
		PMD_INIT_LOG(INFO,
			"virtio: using packed ring %s Tx path on port %u",
			hw->use_vec_tx ? "vectorized" : "standard",
			eth_dev->data->port_id);
		if (hw->use_vec_tx)
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_packed_vec;
		else
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_packed;
	} else {
		if (hw->use_inorder_tx) {
			PMD_INIT_LOG(INFO, "virtio: using inorder Tx path on port %u",
				eth_dev->data->port_id);
			eth_dev->tx_pkt_burst = virtio_xmit_pkts_inorder;
		} else {
			PMD_INIT_LOG(INFO, "virtio: using standard Tx path on port %u",
				eth_dev->data->port_id);
			eth_dev->tx_pkt_burst = virtio_xmit_pkts;
		}
	}

	if (virtio_with_packed_queue(hw)) {
		if (hw->use_vec_rx) {
			PMD_INIT_LOG(INFO,
				"virtio: using packed ring vectorized Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst =
				&virtio_recv_pkts_packed_vec;
		} else if (virtio_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
			PMD_INIT_LOG(INFO,
				"virtio: using packed ring mergeable buffer Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst =
				&virtio_recv_mergeable_pkts_packed;
		} else {
			PMD_INIT_LOG(INFO,
				"virtio: using packed ring standard Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = &virtio_recv_pkts_packed;
		}
	} else {
		if (hw->use_vec_rx) {
			PMD_INIT_LOG(INFO, "virtio: using vectorized Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = virtio_recv_pkts_vec;
		} else if (hw->use_inorder_rx) {
			PMD_INIT_LOG(INFO,
				"virtio: using inorder Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst =	&virtio_recv_pkts_inorder;
		} else if (virtio_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
			PMD_INIT_LOG(INFO,
				"virtio: using mergeable buffer Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = &virtio_recv_mergeable_pkts;
		} else {
			PMD_INIT_LOG(INFO, "virtio: using standard Rx path on port %u",
				eth_dev->data->port_id);
			eth_dev->rx_pkt_burst = &virtio_recv_pkts;
		}
	}

}

/* Only support 1:1 queue/interrupt mapping so far.
 * TODO: support n:1 queue/interrupt mapping when there are limited number of
 * interrupt vectors (<N+1).
 */
static int
virtio_queues_bind_intr(struct rte_eth_dev *dev)
{
	uint32_t i;
	struct virtio_hw *hw = dev->data->dev_private;

	PMD_INIT_LOG(INFO, "queue/interrupt binding");
	for (i = 0; i < dev->data->nb_rx_queues; ++i) {
		if (rte_intr_vec_list_index_set(dev->intr_handle, i,
						       i + 1))
			return -rte_errno;
		if (VIRTIO_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2], i + 1) ==
						 VIRTIO_MSI_NO_VECTOR) {
			PMD_DRV_LOG(ERR, "failed to set queue vector");
			return -EBUSY;
		}
	}

	return 0;
}

static void
virtio_queues_unbind_intr(struct rte_eth_dev *dev)
{
	uint32_t i;
	struct virtio_hw *hw = dev->data->dev_private;

	PMD_INIT_LOG(INFO, "queue/interrupt unbinding");
	for (i = 0; i < dev->data->nb_rx_queues; ++i)
		VIRTIO_OPS(hw)->set_queue_irq(hw,
					     hw->vqs[i * VTNET_CQ],
					     VIRTIO_MSI_NO_VECTOR);
}

static int
virtio_configure_intr(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (!rte_intr_cap_multiple(dev->intr_handle)) {
		PMD_INIT_LOG(ERR, "Multiple intr vector not supported");
		return -ENOTSUP;
	}

	if (rte_intr_efd_enable(dev->intr_handle, dev->data->nb_rx_queues)) {
		PMD_INIT_LOG(ERR, "Fail to create eventfd");
		return -1;
	}

	if (rte_intr_vec_list_alloc(dev->intr_handle, "intr_vec",
				    hw->max_queue_pairs)) {
		PMD_INIT_LOG(ERR, "Failed to allocate %u rxq vectors",
			     hw->max_queue_pairs);
		return -ENOMEM;
	}

	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC) {
		/* Re-register callback to update max_intr */
		rte_intr_callback_unregister(dev->intr_handle,
					     virtio_interrupt_handler,
					     dev);
		rte_intr_callback_register(dev->intr_handle,
					   virtio_interrupt_handler,
					   dev);
	}

	/* DO NOT try to remove this! This function will enable msix, or QEMU
	 * will encounter SIGSEGV when DRIVER_OK is sent.
	 * And for legacy devices, this should be done before queue/vec binding
	 * to change the config size from 20 to 24, or VIRTIO_MSI_QUEUE_VECTOR
	 * (22) will be ignored.
	 */
	if (virtio_intr_enable(dev) < 0) {
		PMD_DRV_LOG(ERR, "interrupt enable failed");
		return -1;
	}

	if (virtio_queues_bind_intr(dev) < 0) {
		PMD_INIT_LOG(ERR, "Failed to bind queue/interrupt");
		return -1;
	}

	return 0;
}

static void
virtio_get_speed_duplex(struct rte_eth_dev *eth_dev,
			struct rte_eth_link *link)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	struct virtio_net_config *config;
	struct virtio_net_config local_config;

	config = &local_config;
	virtio_read_dev_config(hw,
		offsetof(struct virtio_net_config, speed),
		&config->speed, sizeof(config->speed));
	virtio_read_dev_config(hw,
		offsetof(struct virtio_net_config, duplex),
		&config->duplex, sizeof(config->duplex));
	hw->speed = config->speed;
	hw->duplex = config->duplex;
	if (link != NULL) {
		link->link_duplex = hw->duplex;
		link->link_speed  = hw->speed;
	}
	PMD_INIT_LOG(DEBUG, "link speed = %d, duplex = %d",
		     hw->speed, hw->duplex);
}

static uint64_t
ethdev_to_virtio_rss_offloads(uint64_t ethdev_hash_types)
{
	uint64_t virtio_hash_types = 0;

	if (ethdev_hash_types & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
				RTE_ETH_RSS_NONFRAG_IPV4_OTHER))
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_IPV4;

	if (ethdev_hash_types & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_TCPV4;

	if (ethdev_hash_types & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_UDPV4;

	if (ethdev_hash_types & (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
				RTE_ETH_RSS_NONFRAG_IPV6_OTHER))
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_IPV6;

	if (ethdev_hash_types & RTE_ETH_RSS_NONFRAG_IPV6_TCP)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_TCPV6;

	if (ethdev_hash_types & RTE_ETH_RSS_NONFRAG_IPV6_UDP)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_UDPV6;

	if (ethdev_hash_types & RTE_ETH_RSS_IPV6_EX)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_IP_EX;

	if (ethdev_hash_types & RTE_ETH_RSS_IPV6_TCP_EX)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_TCP_EX;

	if (ethdev_hash_types & RTE_ETH_RSS_IPV6_UDP_EX)
		virtio_hash_types |= VIRTIO_NET_HASH_TYPE_UDP_EX;

	return virtio_hash_types;
}

static uint64_t
virtio_to_ethdev_rss_offloads(uint64_t virtio_hash_types)
{
	uint64_t rss_offloads = 0;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_IPV4)
		rss_offloads |= RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
			RTE_ETH_RSS_NONFRAG_IPV4_OTHER;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_TCPV4)
		rss_offloads |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_UDPV4)
		rss_offloads |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_IPV6)
		rss_offloads |= RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
			RTE_ETH_RSS_NONFRAG_IPV6_OTHER;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_TCPV6)
		rss_offloads |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_UDPV6)
		rss_offloads |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_IP_EX)
		rss_offloads |= RTE_ETH_RSS_IPV6_EX;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_TCP_EX)
		rss_offloads |= RTE_ETH_RSS_IPV6_TCP_EX;

	if (virtio_hash_types & VIRTIO_NET_HASH_TYPE_UDP_EX)
		rss_offloads |= RTE_ETH_RSS_IPV6_UDP_EX;

	return rss_offloads;
}

static int
virtio_dev_get_rss_config(struct virtio_hw *hw, uint32_t *rss_hash_types)
{
	struct virtio_net_config local_config;
	struct virtio_net_config *config = &local_config;

	virtio_read_dev_config(hw,
			offsetof(struct virtio_net_config, rss_max_key_size),
			&config->rss_max_key_size,
			sizeof(config->rss_max_key_size));
	if (config->rss_max_key_size < VIRTIO_NET_RSS_KEY_SIZE) {
		PMD_INIT_LOG(ERR, "Invalid device RSS max key size (%u)",
				config->rss_max_key_size);
		return -EINVAL;
	}

	virtio_read_dev_config(hw,
			offsetof(struct virtio_net_config,
				rss_max_indirection_table_length),
			&config->rss_max_indirection_table_length,
			sizeof(config->rss_max_indirection_table_length));
	if (config->rss_max_indirection_table_length < VIRTIO_NET_RSS_RETA_SIZE) {
		PMD_INIT_LOG(ERR, "Invalid device RSS max reta size (%u)",
				config->rss_max_indirection_table_length);
		return -EINVAL;
	}

	virtio_read_dev_config(hw,
			offsetof(struct virtio_net_config, supported_hash_types),
			&config->supported_hash_types,
			sizeof(config->supported_hash_types));
	if ((config->supported_hash_types & VIRTIO_NET_HASH_TYPE_MASK) == 0) {
		PMD_INIT_LOG(ERR, "Invalid device RSS hash types (0x%x)",
				config->supported_hash_types);
		return -EINVAL;
	}

	*rss_hash_types = config->supported_hash_types & VIRTIO_NET_HASH_TYPE_MASK;

	PMD_INIT_LOG(DEBUG, "Device RSS config:");
	PMD_INIT_LOG(DEBUG, "\t-Max key size: %u", config->rss_max_key_size);
	PMD_INIT_LOG(DEBUG, "\t-Max reta size: %u", config->rss_max_indirection_table_length);
	PMD_INIT_LOG(DEBUG, "\t-Supported hash types: 0x%x", *rss_hash_types);

	return 0;
}

static int
virtio_dev_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct virtio_hw *hw = dev->data->dev_private;
	char old_rss_key[VIRTIO_NET_RSS_KEY_SIZE];
	uint32_t old_hash_types;
	uint16_t nb_queues;
	int ret;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_RSS))
		return -ENOTSUP;

	if (rss_conf->rss_hf & ~virtio_to_ethdev_rss_offloads(VIRTIO_NET_HASH_TYPE_MASK))
		return -EINVAL;

	old_hash_types = hw->rss_hash_types;
	hw->rss_hash_types = ethdev_to_virtio_rss_offloads(rss_conf->rss_hf);

	if (rss_conf->rss_key && rss_conf->rss_key_len) {
		if (rss_conf->rss_key_len != VIRTIO_NET_RSS_KEY_SIZE) {
			PMD_INIT_LOG(ERR, "Driver only supports %u RSS key length",
					VIRTIO_NET_RSS_KEY_SIZE);
			ret = -EINVAL;
			goto restore_types;
		}
		memcpy(old_rss_key, hw->rss_key, VIRTIO_NET_RSS_KEY_SIZE);
		memcpy(hw->rss_key, rss_conf->rss_key, VIRTIO_NET_RSS_KEY_SIZE);
	}

	nb_queues = RTE_MAX(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	ret = virtio_set_multiple_queues_rss(dev, nb_queues);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to apply new RSS config to the device");
		goto restore_key;
	}

	return 0;
restore_key:
	if (rss_conf->rss_key && rss_conf->rss_key_len)
		memcpy(hw->rss_key, old_rss_key, VIRTIO_NET_RSS_KEY_SIZE);
restore_types:
	hw->rss_hash_types = old_hash_types;

	return ret;
}

static int
virtio_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_RSS))
		return -ENOTSUP;

	if (rss_conf->rss_key && rss_conf->rss_key_len >= VIRTIO_NET_RSS_KEY_SIZE)
		memcpy(rss_conf->rss_key, hw->rss_key, VIRTIO_NET_RSS_KEY_SIZE);
	rss_conf->rss_key_len = VIRTIO_NET_RSS_KEY_SIZE;
	rss_conf->rss_hf = virtio_to_ethdev_rss_offloads(hw->rss_hash_types);

	return 0;
}

static int virtio_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint16_t nb_queues;
	uint16_t old_reta[VIRTIO_NET_RSS_RETA_SIZE];
	int idx, pos, i, ret;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_RSS))
		return -ENOTSUP;

	if (reta_size != VIRTIO_NET_RSS_RETA_SIZE)
		return -EINVAL;

	memcpy(old_reta, hw->rss_reta, sizeof(old_reta));

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		pos = i % RTE_ETH_RETA_GROUP_SIZE;

		if (((reta_conf[idx].mask >> pos) & 0x1) == 0)
			continue;

		hw->rss_reta[i] = reta_conf[idx].reta[pos];
	}

	nb_queues = RTE_MAX(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	ret = virtio_set_multiple_queues_rss(dev, nb_queues);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to apply new RETA to the device");
		memcpy(hw->rss_reta, old_reta, sizeof(old_reta));
	}

	hw->rss_rx_queues = dev->data->nb_rx_queues;

	return ret;
}

static int virtio_dev_rss_reta_query(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct virtio_hw *hw = dev->data->dev_private;
	int idx, i;

	if (!virtio_with_feature(hw, VIRTIO_NET_F_RSS))
		return -ENOTSUP;

	if (reta_size != VIRTIO_NET_RSS_RETA_SIZE)
		return -EINVAL;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		reta_conf[idx].reta[i % RTE_ETH_RETA_GROUP_SIZE] = hw->rss_reta[i];
	}

	return 0;
}

/*
 * As default RSS hash key, it uses the default key of the
 * Intel IXGBE devices. It can be updated by the application
 * with any 40B key value.
 */
static uint8_t rss_intel_key[VIRTIO_NET_RSS_KEY_SIZE] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
};

static int
virtio_dev_rss_init(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	uint16_t nb_rx_queues = eth_dev->data->nb_rx_queues;
	struct rte_eth_rss_conf *rss_conf;
	int ret, i;

	if (!nb_rx_queues) {
		PMD_INIT_LOG(ERR, "Cannot init RSS if no Rx queues");
		return -EINVAL;
	}

	rss_conf = &eth_dev->data->dev_conf.rx_adv_conf.rss_conf;

	ret = virtio_dev_get_rss_config(hw, &hw->rss_hash_types);
	if (ret)
		return ret;

	if (rss_conf->rss_hf) {
		/*  Ensure requested hash types are supported by the device */
		if (rss_conf->rss_hf & ~virtio_to_ethdev_rss_offloads(hw->rss_hash_types))
			return -EINVAL;

		hw->rss_hash_types = ethdev_to_virtio_rss_offloads(rss_conf->rss_hf);
	}

	if (!hw->rss_key) {
		/* Setup default RSS key if not already setup by the user */
		hw->rss_key = rte_malloc_socket("rss_key",
				VIRTIO_NET_RSS_KEY_SIZE, 0,
				eth_dev->device->numa_node);
		if (!hw->rss_key) {
			PMD_INIT_LOG(ERR, "Failed to allocate RSS key");
			return -1;
		}
	}

	if (rss_conf->rss_key && rss_conf->rss_key_len) {
		if (rss_conf->rss_key_len != VIRTIO_NET_RSS_KEY_SIZE) {
			PMD_INIT_LOG(ERR, "Driver only supports %u RSS key length",
					VIRTIO_NET_RSS_KEY_SIZE);
			return -EINVAL;
		}
		memcpy(hw->rss_key, rss_conf->rss_key, VIRTIO_NET_RSS_KEY_SIZE);
	} else {
		memcpy(hw->rss_key, rss_intel_key, VIRTIO_NET_RSS_KEY_SIZE);
	}

	if (!hw->rss_reta) {
		/* Setup default RSS reta if not already setup by the user */
		hw->rss_reta = rte_zmalloc_socket("rss_reta",
				VIRTIO_NET_RSS_RETA_SIZE * sizeof(uint16_t), 0,
				eth_dev->device->numa_node);
		if (!hw->rss_reta) {
			PMD_INIT_LOG(ERR, "Failed to allocate RSS reta");
			return -1;
		}

		hw->rss_rx_queues = 0;
	}

	/* Re-initialize the RSS reta if the number of RX queues has changed */
	if (hw->rss_rx_queues != nb_rx_queues) {
		for (i = 0; i < VIRTIO_NET_RSS_RETA_SIZE; i++)
			hw->rss_reta[i] = i % nb_rx_queues;
		hw->rss_rx_queues = nb_rx_queues;
	}

	return 0;
}

#define DUPLEX_UNKNOWN   0xff
/* reset device and renegotiate features if needed */
static int
virtio_init_device(struct rte_eth_dev *eth_dev, uint64_t req_features)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	struct virtio_net_config *config;
	struct virtio_net_config local_config;
	int ret;

	/* Reset the device although not necessary at startup */
	virtio_reset(hw);

	if (hw->vqs) {
		virtio_dev_free_mbufs(eth_dev);
		virtio_free_queues(hw);
	}

	/* Tell the host we've noticed this device. */
	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_ACK);

	/* Tell the host we've known how to drive the device. */
	virtio_set_status(hw, VIRTIO_CONFIG_STATUS_DRIVER);
	if (virtio_ethdev_negotiate_features(hw, req_features) < 0)
		return -1;

	hw->weak_barriers = !virtio_with_feature(hw, VIRTIO_F_ORDER_PLATFORM);

	/* If host does not support both status and MSI-X then disable LSC */
	if (virtio_with_feature(hw, VIRTIO_NET_F_STATUS) && hw->intr_lsc)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	else
		eth_dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	/* Setting up rx_header size for the device */
	if (virtio_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF) ||
	    virtio_with_feature(hw, VIRTIO_F_VERSION_1) ||
	    virtio_with_packed_queue(hw))
		hw->vtnet_hdr_size = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	else
		hw->vtnet_hdr_size = sizeof(struct virtio_net_hdr);

	/* Copy the permanent MAC address to: virtio_hw */
	virtio_get_hwaddr(hw);
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr,
			&eth_dev->data->mac_addrs[0]);
	PMD_INIT_LOG(DEBUG,
		     "PORT MAC: " RTE_ETHER_ADDR_PRT_FMT,
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	hw->get_speed_via_feat = hw->speed == RTE_ETH_SPEED_NUM_UNKNOWN &&
			     virtio_with_feature(hw, VIRTIO_NET_F_SPEED_DUPLEX);
	if (hw->get_speed_via_feat)
		virtio_get_speed_duplex(eth_dev, NULL);
	if (hw->duplex == DUPLEX_UNKNOWN)
		hw->duplex = RTE_ETH_LINK_FULL_DUPLEX;
	PMD_INIT_LOG(DEBUG, "link speed = %d, duplex = %d",
		hw->speed, hw->duplex);
	if (virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VQ)) {
		config = &local_config;

		virtio_read_dev_config(hw,
			offsetof(struct virtio_net_config, mac),
			&config->mac, sizeof(config->mac));

		if (virtio_with_feature(hw, VIRTIO_NET_F_STATUS)) {
			virtio_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&config->status, sizeof(config->status));
		} else {
			PMD_INIT_LOG(DEBUG,
				     "VIRTIO_NET_F_STATUS is not supported");
			config->status = 0;
		}

		if (virtio_with_feature(hw, VIRTIO_NET_F_MQ) ||
				virtio_with_feature(hw, VIRTIO_NET_F_RSS)) {
			virtio_read_dev_config(hw,
				offsetof(struct virtio_net_config, max_virtqueue_pairs),
				&config->max_virtqueue_pairs,
				sizeof(config->max_virtqueue_pairs));
		} else {
			PMD_INIT_LOG(DEBUG,
				     "Neither VIRTIO_NET_F_MQ nor VIRTIO_NET_F_RSS are supported");
			config->max_virtqueue_pairs = 1;
		}

		hw->max_queue_pairs = config->max_virtqueue_pairs;

		if (virtio_with_feature(hw, VIRTIO_NET_F_MTU)) {
			virtio_read_dev_config(hw,
				offsetof(struct virtio_net_config, mtu),
				&config->mtu,
				sizeof(config->mtu));

			/*
			 * MTU value has already been checked at negotiation
			 * time, but check again in case it has changed since
			 * then, which should not happen.
			 */
			if (config->mtu < RTE_ETHER_MIN_MTU) {
				PMD_INIT_LOG(ERR, "invalid max MTU value (%u)",
						config->mtu);
				return -1;
			}

			hw->max_mtu = config->mtu;
			/* Set initial MTU to maximum one supported by vhost */
			eth_dev->data->mtu = config->mtu;

		} else {
			hw->max_mtu = VIRTIO_MAX_RX_PKTLEN - RTE_ETHER_HDR_LEN -
				VLAN_TAG_LEN - hw->vtnet_hdr_size;
		}

		hw->rss_hash_types = 0;
		if (virtio_with_feature(hw, VIRTIO_NET_F_RSS))
			if (virtio_dev_rss_init(eth_dev))
				return -1;

		PMD_INIT_LOG(DEBUG, "config->max_virtqueue_pairs=%d",
				config->max_virtqueue_pairs);
		PMD_INIT_LOG(DEBUG, "config->status=%d", config->status);
		PMD_INIT_LOG(DEBUG,
				"PORT MAC: " RTE_ETHER_ADDR_PRT_FMT,
				config->mac[0], config->mac[1],
				config->mac[2], config->mac[3],
				config->mac[4], config->mac[5]);
	} else {
		PMD_INIT_LOG(DEBUG, "config->max_virtqueue_pairs=1");
		hw->max_queue_pairs = 1;
		hw->max_mtu = VIRTIO_MAX_RX_PKTLEN - RTE_ETHER_HDR_LEN -
			VLAN_TAG_LEN - hw->vtnet_hdr_size;
	}

	ret = virtio_alloc_queues(eth_dev);
	if (ret < 0)
		return ret;

	if (eth_dev->data->dev_conf.intr_conf.rxq) {
		if (virtio_configure_intr(eth_dev) < 0) {
			PMD_INIT_LOG(ERR, "failed to configure interrupt");
			virtio_free_queues(hw);
			return -1;
		}
	}

	virtio_reinit_complete(hw);

	return 0;
}

/*
 * This function is based on probe() function in virtio_pci.c
 * It returns 0 on success.
 */
int
eth_virtio_dev_init(struct rte_eth_dev *eth_dev)
{
	struct virtio_hw *hw = eth_dev->data->dev_private;
	uint32_t speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	int vectorized = 0;
	int ret;

	if (sizeof(struct virtio_net_hdr_mrg_rxbuf) > RTE_PKTMBUF_HEADROOM) {
		PMD_INIT_LOG(ERR,
			"Not sufficient headroom required = %d, avail = %d",
			(int)sizeof(struct virtio_net_hdr_mrg_rxbuf),
			RTE_PKTMBUF_HEADROOM);

		return -1;
	}

	eth_dev->dev_ops = &virtio_eth_dev_ops;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		set_rxtx_funcs(eth_dev);
		return 0;
	}

	ret = virtio_dev_devargs_parse(eth_dev->device->devargs, &speed, &vectorized);
	if (ret < 0)
		return ret;
	hw->speed = speed;
	hw->duplex = DUPLEX_UNKNOWN;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("virtio",
				VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			VIRTIO_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	rte_spinlock_init(&hw->state_lock);

	if (vectorized) {
		hw->use_vec_rx = 1;
		hw->use_vec_tx = 1;
	}

	/* reset device and negotiate default features */
	ret = virtio_init_device(eth_dev, VIRTIO_PMD_DEFAULT_GUEST_FEATURES);
	if (ret < 0)
		goto err_virtio_init;

	if (vectorized) {
		if (!virtio_with_packed_queue(hw)) {
			hw->use_vec_tx = 0;
		} else {
#if !defined(CC_AVX512_SUPPORT) && !defined(RTE_ARCH_ARM)
			hw->use_vec_rx = 0;
			hw->use_vec_tx = 0;
			PMD_DRV_LOG(INFO,
				"building environment do not support packed ring vectorized");
#endif
		}
	}

	hw->opened = 1;

	return 0;

err_virtio_init:
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;
	return ret;
}

static uint32_t
virtio_dev_speed_capa_get(uint32_t speed)
{
	switch (speed) {
	case RTE_ETH_SPEED_NUM_10G:
		return RTE_ETH_LINK_SPEED_10G;
	case RTE_ETH_SPEED_NUM_20G:
		return RTE_ETH_LINK_SPEED_20G;
	case RTE_ETH_SPEED_NUM_25G:
		return RTE_ETH_LINK_SPEED_25G;
	case RTE_ETH_SPEED_NUM_40G:
		return RTE_ETH_LINK_SPEED_40G;
	case RTE_ETH_SPEED_NUM_50G:
		return RTE_ETH_LINK_SPEED_50G;
	case RTE_ETH_SPEED_NUM_56G:
		return RTE_ETH_LINK_SPEED_56G;
	case RTE_ETH_SPEED_NUM_100G:
		return RTE_ETH_LINK_SPEED_100G;
	case RTE_ETH_SPEED_NUM_200G:
		return RTE_ETH_LINK_SPEED_200G;
	case RTE_ETH_SPEED_NUM_400G:
		return RTE_ETH_LINK_SPEED_400G;
	default:
		return 0;
	}
}

static int vectorized_check_handler(__rte_unused const char *key,
		const char *value, void *ret_val)
{
	if (value == NULL || ret_val == NULL)
		return -EINVAL;

	if (strcmp(value, "1") == 0)
		*(int *)ret_val = 1;
	else
		*(int *)ret_val = 0;

	return 0;
}

#define VIRTIO_ARG_SPEED      "speed"
#define VIRTIO_ARG_VECTORIZED "vectorized"

static int
link_speed_handler(const char *key __rte_unused,
		const char *value, void *ret_val)
{
	uint32_t val;
	if (!value || !ret_val)
		return -EINVAL;
	val = strtoul(value, NULL, 0);
	/* validate input */
	if (virtio_dev_speed_capa_get(val) == 0)
		return -EINVAL;
	*(uint32_t *)ret_val = val;

	return 0;
}


static int
virtio_dev_devargs_parse(struct rte_devargs *devargs, uint32_t *speed, int *vectorized)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "error when parsing param");
		return 0;
	}

	if (speed && rte_kvargs_count(kvlist, VIRTIO_ARG_SPEED) == 1) {
		ret = rte_kvargs_process(kvlist,
					VIRTIO_ARG_SPEED,
					link_speed_handler, speed);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to parse %s",
					VIRTIO_ARG_SPEED);
			goto exit;
		}
	}

	if (vectorized &&
		rte_kvargs_count(kvlist, VIRTIO_ARG_VECTORIZED) == 1) {
		ret = rte_kvargs_process(kvlist,
				VIRTIO_ARG_VECTORIZED,
				vectorized_check_handler, vectorized);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to parse %s",
					VIRTIO_ARG_VECTORIZED);
			goto exit;
		}
	}

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static uint8_t
rx_offload_enabled(struct virtio_hw *hw)
{
	return virtio_with_feature(hw, VIRTIO_NET_F_GUEST_CSUM) ||
		virtio_with_feature(hw, VIRTIO_NET_F_GUEST_TSO4) ||
		virtio_with_feature(hw, VIRTIO_NET_F_GUEST_TSO6);
}

static uint8_t
tx_offload_enabled(struct virtio_hw *hw)
{
	return virtio_with_feature(hw, VIRTIO_NET_F_CSUM) ||
		virtio_with_feature(hw, VIRTIO_NET_F_HOST_TSO4) ||
		virtio_with_feature(hw, VIRTIO_NET_F_HOST_TSO6);
}

/*
 * Configure virtio device
 * It returns 0 on success.
 */
static int
virtio_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	const struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;
	struct virtio_hw *hw = dev->data->dev_private;
	uint32_t ether_hdr_len = RTE_ETHER_HDR_LEN + VLAN_TAG_LEN +
		hw->vtnet_hdr_size;
	uint64_t rx_offloads = rxmode->offloads;
	uint64_t tx_offloads = txmode->offloads;
	uint64_t req_features;
	int ret;

	PMD_INIT_LOG(DEBUG, "configure");
	req_features = VIRTIO_PMD_DEFAULT_GUEST_FEATURES;

	if (rxmode->mq_mode != RTE_ETH_MQ_RX_NONE && rxmode->mq_mode != RTE_ETH_MQ_RX_RSS) {
		PMD_DRV_LOG(ERR,
			"Unsupported Rx multi queue mode %d",
			rxmode->mq_mode);
		return -EINVAL;
	}

	if (txmode->mq_mode != RTE_ETH_MQ_TX_NONE) {
		PMD_DRV_LOG(ERR,
			"Unsupported Tx multi queue mode %d",
			txmode->mq_mode);
		return -EINVAL;
	}

	if (dev->data->dev_conf.intr_conf.rxq) {
		ret = virtio_init_device(dev, hw->req_guest_features);
		if (ret < 0)
			return ret;
	}

	if (rxmode->mq_mode == RTE_ETH_MQ_RX_RSS)
		req_features |= (1ULL << VIRTIO_NET_F_RSS);

	if (rxmode->mtu > hw->max_mtu)
		req_features &= ~(1ULL << VIRTIO_NET_F_MTU);

	hw->max_rx_pkt_len = ether_hdr_len + rxmode->mtu;

	if (rx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
			   RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
		req_features |= (1ULL << VIRTIO_NET_F_GUEST_CSUM);

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		req_features |=
			(1ULL << VIRTIO_NET_F_GUEST_TSO4) |
			(1ULL << VIRTIO_NET_F_GUEST_TSO6);

	if (tx_offloads & (RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
			   RTE_ETH_TX_OFFLOAD_TCP_CKSUM))
		req_features |= (1ULL << VIRTIO_NET_F_CSUM);

	if (tx_offloads & RTE_ETH_TX_OFFLOAD_TCP_TSO)
		req_features |=
			(1ULL << VIRTIO_NET_F_HOST_TSO4) |
			(1ULL << VIRTIO_NET_F_HOST_TSO6);

	/* if request features changed, reinit the device */
	if (req_features != hw->req_guest_features) {
		ret = virtio_init_device(dev, req_features);
		if (ret < 0)
			return ret;
	}

	/* if queues are not allocated, reinit the device */
	if (hw->vqs == NULL) {
		ret = virtio_init_device(dev, hw->req_guest_features);
		if (ret < 0)
			return ret;
	}

	if ((rxmode->mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) &&
			!virtio_with_feature(hw, VIRTIO_NET_F_RSS)) {
		PMD_DRV_LOG(ERR, "RSS support requested but not supported by the device");
		return -ENOTSUP;
	}

	if ((rx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
			    RTE_ETH_RX_OFFLOAD_TCP_CKSUM)) &&
		!virtio_with_feature(hw, VIRTIO_NET_F_GUEST_CSUM)) {
		PMD_DRV_LOG(ERR,
			"rx checksum not available on this host");
		return -ENOTSUP;
	}

	if ((rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) &&
		(!virtio_with_feature(hw, VIRTIO_NET_F_GUEST_TSO4) ||
		 !virtio_with_feature(hw, VIRTIO_NET_F_GUEST_TSO6))) {
		PMD_DRV_LOG(ERR,
			"Large Receive Offload not available on this host");
		return -ENOTSUP;
	}

	/* start control queue */
	if (virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VQ))
		virtio_dev_cq_start(dev);

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		hw->vlan_strip = 1;

	hw->rx_ol_scatter = (rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER);

	if ((rx_offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) &&
			!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN)) {
		PMD_DRV_LOG(ERR,
			    "vlan filtering not available on this host");
		return -ENOTSUP;
	}

	hw->has_tx_offload = tx_offload_enabled(hw);
	hw->has_rx_offload = rx_offload_enabled(hw);

	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		/* Enable vector (0) for Link State Interrupt */
		if (VIRTIO_OPS(hw)->set_config_irq(hw, 0) ==
				VIRTIO_MSI_NO_VECTOR) {
			PMD_DRV_LOG(ERR, "failed to set config vector");
			return -EBUSY;
		}

	if (virtio_with_packed_queue(hw)) {
#if defined(RTE_ARCH_X86_64) && defined(CC_AVX512_SUPPORT)
		if ((hw->use_vec_rx || hw->use_vec_tx) &&
		    (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) ||
		     !virtio_with_feature(hw, VIRTIO_F_IN_ORDER) ||
		     !virtio_with_feature(hw, VIRTIO_F_VERSION_1) ||
		     rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_512)) {
			PMD_DRV_LOG(INFO,
				"disabled packed ring vectorized path for requirements not met");
			hw->use_vec_rx = 0;
			hw->use_vec_tx = 0;
		}
#elif defined(RTE_ARCH_ARM)
		if ((hw->use_vec_rx || hw->use_vec_tx) &&
		    (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON) ||
		     !virtio_with_feature(hw, VIRTIO_F_IN_ORDER) ||
		     !virtio_with_feature(hw, VIRTIO_F_VERSION_1) ||
		     rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128)) {
			PMD_DRV_LOG(INFO,
				"disabled packed ring vectorized path for requirements not met");
			hw->use_vec_rx = 0;
			hw->use_vec_tx = 0;
		}
#else
		hw->use_vec_rx = 0;
		hw->use_vec_tx = 0;
#endif

		if (hw->use_vec_rx) {
			if (virtio_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
				PMD_DRV_LOG(INFO,
					"disabled packed ring vectorized rx for mrg_rxbuf enabled");
				hw->use_vec_rx = 0;
			}

			if (rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) {
				PMD_DRV_LOG(INFO,
					"disabled packed ring vectorized rx for TCP_LRO enabled");
				hw->use_vec_rx = 0;
			}
		}
	} else {
		if (virtio_with_feature(hw, VIRTIO_F_IN_ORDER)) {
			hw->use_inorder_tx = 1;
			hw->use_inorder_rx = 1;
			hw->use_vec_rx = 0;
		}

		if (hw->use_vec_rx) {
#if defined RTE_ARCH_ARM
			if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON)) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized path for requirement not met");
				hw->use_vec_rx = 0;
			}
#endif
			if (virtio_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized rx for mrg_rxbuf enabled");
				hw->use_vec_rx = 0;
			}

			if (rx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
					   RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
					   RTE_ETH_RX_OFFLOAD_TCP_LRO |
					   RTE_ETH_RX_OFFLOAD_VLAN_STRIP)) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized rx for offloading enabled");
				hw->use_vec_rx = 0;
			}

			if (rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128) {
				PMD_DRV_LOG(INFO,
					"disabled split ring vectorized rx, max SIMD bitwidth too low");
				hw->use_vec_rx = 0;
			}
		}
	}

	return 0;
}


static int
virtio_dev_start(struct rte_eth_dev *dev)
{
	uint16_t nb_queues, i;
	struct virtqueue *vq;
	struct virtio_hw *hw = dev->data->dev_private;
	int ret;

	/* Finish the initialization of the queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ret = virtio_dev_rx_queue_setup_finish(dev, i);
		if (ret < 0)
			return ret;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ret = virtio_dev_tx_queue_setup_finish(dev, i);
		if (ret < 0)
			return ret;
	}

	/* check if lsc interrupt feature is enabled */
	if (dev->data->dev_conf.intr_conf.lsc) {
		if (!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)) {
			PMD_DRV_LOG(ERR, "link status not supported by host");
			return -ENOTSUP;
		}
	}

	/* Enable uio/vfio intr/eventfd mapping: although we already did that
	 * in device configure, but it could be unmapped  when device is
	 * stopped.
	 */
	if (dev->data->dev_conf.intr_conf.lsc ||
	    dev->data->dev_conf.intr_conf.rxq) {
		virtio_intr_disable(dev);

		/* Setup interrupt callback  */
		if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
			rte_intr_callback_register(dev->intr_handle,
						   virtio_interrupt_handler,
						   dev);

		if (virtio_intr_enable(dev) < 0) {
			PMD_DRV_LOG(ERR, "interrupt enable failed");
			return -EIO;
		}
	}

	/*Notify the backend
	 *Otherwise the tap backend might already stop its queue due to fullness.
	 *vhost backend will have no chance to be waked up
	 */
	nb_queues = RTE_MAX(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	if (hw->max_queue_pairs > 1) {
		if (virtio_set_multiple_queues(dev, nb_queues) != 0)
			return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "nb_queues=%u (port=%u)", nb_queues,
		     dev->data->port_id);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		vq = virtnet_rxq_to_vq(dev->data->rx_queues[i]);
		/* Flush the old packets */
		virtqueue_rxvq_flush(vq);
		virtqueue_notify(vq);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		vq = virtnet_txq_to_vq(dev->data->tx_queues[i]);
		virtqueue_notify(vq);
	}

	PMD_INIT_LOG(DEBUG, "Notified backend at initialization (port=%u)",
		     dev->data->port_id);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		vq = virtnet_rxq_to_vq(dev->data->rx_queues[i]);
		VIRTQUEUE_DUMP(vq);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		vq = virtnet_txq_to_vq(dev->data->tx_queues[i]);
		VIRTQUEUE_DUMP(vq);
	}

	set_rxtx_funcs(dev);
	hw->started = 1;

	/* Initialize Link state */
	virtio_dev_link_update(dev, 0);

	return 0;
}

static void virtio_dev_free_mbufs(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = virtio_get_nr_vq(hw);
	const char *type __rte_unused;
	unsigned int i, mbuf_num = 0;
	struct virtqueue *vq;
	struct rte_mbuf *buf;
	int queue_type;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (!vq)
			continue;

		queue_type = virtio_get_queue_type(hw, i);
		if (queue_type == VTNET_RQ)
			type = "rxq";
		else if (queue_type == VTNET_TQ)
			type = "txq";
		else
			continue;

		PMD_INIT_LOG(DEBUG,
			"Before freeing %s[%d] used and unused buf",
			type, i);
		VIRTQUEUE_DUMP(vq);

		while ((buf = virtqueue_detach_unused(vq)) != NULL) {
			rte_pktmbuf_free(buf);
			mbuf_num++;
		}

		PMD_INIT_LOG(DEBUG,
			"After freeing %s[%d] used and unused buf",
			type, i);
		VIRTQUEUE_DUMP(vq);
	}

	PMD_INIT_LOG(DEBUG, "%d mbufs freed", mbuf_num);
}

static void
virtio_tx_completed_cleanup(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq;
	int qidx;
	void (*xmit_cleanup)(struct virtqueue *vq, uint16_t nb_used);

	if (virtio_with_packed_queue(hw)) {
		if (hw->use_vec_tx)
			xmit_cleanup = &virtio_xmit_cleanup_inorder_packed;
		else if (virtio_with_feature(hw, VIRTIO_F_IN_ORDER))
			xmit_cleanup = &virtio_xmit_cleanup_inorder_packed;
		else
			xmit_cleanup = &virtio_xmit_cleanup_normal_packed;
	} else {
		if (hw->use_inorder_tx)
			xmit_cleanup = &virtio_xmit_cleanup_inorder;
		else
			xmit_cleanup = &virtio_xmit_cleanup;
	}

	for (qidx = 0; qidx < hw->max_queue_pairs; qidx++) {
		vq = hw->vqs[2 * qidx + VTNET_SQ_TQ_QUEUE_IDX];
		if (vq != NULL)
			xmit_cleanup(vq, virtqueue_nused(vq));
	}
}

/*
 * Stop device: disable interrupt and mark link down
 */
int
virtio_dev_stop(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;
	struct rte_eth_link link;
	struct rte_eth_intr_conf *intr_conf = &dev->data->dev_conf.intr_conf;

	PMD_INIT_LOG(DEBUG, "stop");
	dev->data->dev_started = 0;

	rte_spinlock_lock(&hw->state_lock);
	if (!hw->started)
		goto out_unlock;
	hw->started = 0;

	virtio_tx_completed_cleanup(dev);

	if (intr_conf->lsc || intr_conf->rxq) {
		virtio_intr_disable(dev);

		/* Reset interrupt callback  */
		if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC) {
			rte_intr_callback_unregister(dev->intr_handle,
						     virtio_interrupt_handler,
						     dev);
		}
	}

	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);
out_unlock:
	rte_spinlock_unlock(&hw->state_lock);

	return 0;
}

static int
virtio_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct rte_eth_link link;
	uint16_t status;
	struct virtio_hw *hw = dev->data->dev_private;

	memset(&link, 0, sizeof(link));
	link.link_duplex = hw->duplex;
	link.link_speed  = hw->speed;
	link.link_autoneg = RTE_ETH_LINK_AUTONEG;

	if (!hw->started) {
		link.link_status = RTE_ETH_LINK_DOWN;
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	} else if (virtio_with_feature(hw, VIRTIO_NET_F_STATUS)) {
		PMD_INIT_LOG(DEBUG, "Get link status from hw");
		virtio_read_dev_config(hw,
				offsetof(struct virtio_net_config, status),
				&status, sizeof(status));
		if ((status & VIRTIO_NET_S_LINK_UP) == 0) {
			link.link_status = RTE_ETH_LINK_DOWN;
			link.link_speed = RTE_ETH_SPEED_NUM_NONE;
			PMD_INIT_LOG(DEBUG, "Port %d is down",
				     dev->data->port_id);
		} else {
			link.link_status = RTE_ETH_LINK_UP;
			if (hw->get_speed_via_feat)
				virtio_get_speed_duplex(dev, &link);
			PMD_INIT_LOG(DEBUG, "Port %d is up",
				     dev->data->port_id);
		}
	} else {
		link.link_status = RTE_ETH_LINK_UP;
		if (hw->get_speed_via_feat)
			virtio_get_speed_duplex(dev, &link);
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
virtio_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct virtio_hw *hw = dev->data->dev_private;
	uint64_t offloads = rxmode->offloads;

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if ((offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) &&
				!virtio_with_feature(hw, VIRTIO_NET_F_CTRL_VLAN)) {

			PMD_DRV_LOG(NOTICE,
				"vlan filtering not available on this host");

			return -ENOTSUP;
		}
	}

	if (mask & RTE_ETH_VLAN_STRIP_MASK)
		hw->vlan_strip = !!(offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP);

	return 0;
}

static int
virtio_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	uint64_t tso_mask, host_features;
	uint32_t rss_hash_types = 0;
	struct virtio_hw *hw = dev->data->dev_private;
	dev_info->speed_capa = virtio_dev_speed_capa_get(hw->speed);

	dev_info->max_rx_queues =
		RTE_MIN(hw->max_queue_pairs, VIRTIO_MAX_RX_QUEUES);
	dev_info->max_tx_queues =
		RTE_MIN(hw->max_queue_pairs, VIRTIO_MAX_TX_QUEUES);
	dev_info->min_rx_bufsize = VIRTIO_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen = VIRTIO_MAX_RX_PKTLEN;
	dev_info->max_mac_addrs = VIRTIO_MAX_MAC_ADDRS;
	dev_info->max_mtu = hw->max_mtu;

	host_features = VIRTIO_OPS(hw)->get_features(hw);
	dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	if (host_features & (1ULL << VIRTIO_NET_F_MRG_RXBUF))
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_SCATTER;
	if (host_features & (1ULL << VIRTIO_NET_F_GUEST_CSUM)) {
		dev_info->rx_offload_capa |=
			RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
			RTE_ETH_RX_OFFLOAD_UDP_CKSUM;
	}
	if (host_features & (1ULL << VIRTIO_NET_F_CTRL_VLAN))
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
	tso_mask = (1ULL << VIRTIO_NET_F_GUEST_TSO4) |
		(1ULL << VIRTIO_NET_F_GUEST_TSO6);
	if ((host_features & tso_mask) == tso_mask)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_TCP_LRO;

	dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
				    RTE_ETH_TX_OFFLOAD_VLAN_INSERT;
	if (host_features & (1ULL << VIRTIO_NET_F_CSUM)) {
		dev_info->tx_offload_capa |=
			RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
			RTE_ETH_TX_OFFLOAD_TCP_CKSUM;
	}
	tso_mask = (1ULL << VIRTIO_NET_F_HOST_TSO4) |
		(1ULL << VIRTIO_NET_F_HOST_TSO6);
	if ((host_features & tso_mask) == tso_mask)
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_TCP_TSO;

	if (host_features & (1ULL << VIRTIO_NET_F_RSS)) {
		virtio_dev_get_rss_config(hw, &rss_hash_types);
		dev_info->hash_key_size = VIRTIO_NET_RSS_KEY_SIZE;
		dev_info->reta_size = VIRTIO_NET_RSS_RETA_SIZE;
		dev_info->flow_type_rss_offloads =
			virtio_to_ethdev_rss_offloads(rss_hash_types);
	} else {
		dev_info->hash_key_size = 0;
		dev_info->reta_size = 0;
		dev_info->flow_type_rss_offloads = 0;
	}

	if (host_features & (1ULL << VIRTIO_F_RING_PACKED)) {
		/*
		 * According to 2.7 Packed Virtqueues,
		 * 2.7.10.1 Structure Size and Alignment:
		 * The Queue Size value does not have to be a power of 2.
		 */
		dev_info->rx_desc_lim.nb_max = UINT16_MAX;
		dev_info->tx_desc_lim.nb_max = UINT16_MAX;
	} else {
		/*
		 * According to 2.6 Split Virtqueues:
		 * Queue Size value is always a power of 2. The maximum Queue
		 * Size value is 32768.
		 */
		dev_info->rx_desc_lim.nb_max = 32768;
		dev_info->tx_desc_lim.nb_max = 32768;
	}
	/*
	 * Actual minimum is not the same for virtqueues of different kinds,
	 * but to avoid tangling the code with separate branches, rely on
	 * default thresholds since desc number must be at least of their size.
	 */
	dev_info->rx_desc_lim.nb_min = RTE_MAX(DEFAULT_RX_FREE_THRESH,
					       RTE_VIRTIO_VPMD_RX_REARM_THRESH);
	dev_info->tx_desc_lim.nb_min = DEFAULT_TX_FREE_THRESH;
	dev_info->rx_desc_lim.nb_align = 1;
	dev_info->tx_desc_lim.nb_align = 1;

	return 0;
}

/*
 * It enables testpmd to collect per queue stats.
 */
static int
virtio_dev_queue_stats_mapping_set(__rte_unused struct rte_eth_dev *eth_dev,
__rte_unused uint16_t queue_id, __rte_unused uint8_t stat_idx,
__rte_unused uint8_t is_rx)
{
	return 0;
}

RTE_LOG_REGISTER_SUFFIX(virtio_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(virtio_logtype_driver, driver, NOTICE);
