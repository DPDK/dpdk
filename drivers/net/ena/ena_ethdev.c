/*-
* BSD LICENSE
*
* Copyright (c) 2015-2016 Amazon.com, Inc. or its affiliates.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in
* the documentation and/or other materials provided with the
* distribution.
* * Neither the name of copyright holder nor the names of its
* contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_tcp.h>
#include <rte_atomic.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_version.h>
#include <rte_eal_memconfig.h>
#include <rte_net.h>

#include "ena_ethdev.h"
#include "ena_logs.h"
#include "ena_platform.h"
#include "ena_com.h"
#include "ena_eth_com.h"

#include <ena_common_defs.h>
#include <ena_regs_defs.h>
#include <ena_admin_defs.h>
#include <ena_eth_io_defs.h>

#define DRV_MODULE_VER_MAJOR	1
#define DRV_MODULE_VER_MINOR	0
#define DRV_MODULE_VER_SUBMINOR	0

#define ENA_IO_TXQ_IDX(q)	(2 * (q))
#define ENA_IO_RXQ_IDX(q)	(2 * (q) + 1)
/*reverse version of ENA_IO_RXQ_IDX*/
#define ENA_IO_RXQ_IDX_REV(q)	((q - 1) / 2)

/* While processing submitted and completed descriptors (rx and tx path
 * respectively) in a loop it is desired to:
 *  - perform batch submissions while populating sumbissmion queue
 *  - avoid blocking transmission of other packets during cleanup phase
 * Hence the utilization ratio of 1/8 of a queue size.
 */
#define ENA_RING_DESCS_RATIO(ring_size)	(ring_size / 8)

#define __MERGE_64B_H_L(h, l) (((uint64_t)h << 32) | l)
#define TEST_BIT(val, bit_shift) (val & (1UL << bit_shift))

#define GET_L4_HDR_LEN(mbuf)					\
	((rte_pktmbuf_mtod_offset(mbuf,	struct tcp_hdr *,	\
		mbuf->l3_len + mbuf->l2_len)->data_off) >> 4)

#define ENA_RX_RSS_TABLE_LOG_SIZE  7
#define ENA_RX_RSS_TABLE_SIZE	(1 << ENA_RX_RSS_TABLE_LOG_SIZE)
#define ENA_HASH_KEY_SIZE	40
#define ENA_ETH_SS_STATS	0xFF
#define ETH_GSTRING_LEN	32

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum ethtool_stringset {
	ETH_SS_TEST             = 0,
	ETH_SS_STATS,
};

struct ena_stats {
	char name[ETH_GSTRING_LEN];
	int stat_offset;
};

#define ENA_STAT_ENA_COM_ENTRY(stat) { \
	.name = #stat, \
	.stat_offset = offsetof(struct ena_com_stats_admin, stat) \
}

#define ENA_STAT_ENTRY(stat, stat_type) { \
	.name = #stat, \
	.stat_offset = offsetof(struct ena_stats_##stat_type, stat) \
}

#define ENA_STAT_RX_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, rx)

#define ENA_STAT_TX_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, tx)

#define ENA_STAT_GLOBAL_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, dev)

static const struct ena_stats ena_stats_global_strings[] = {
	ENA_STAT_GLOBAL_ENTRY(tx_timeout),
	ENA_STAT_GLOBAL_ENTRY(io_suspend),
	ENA_STAT_GLOBAL_ENTRY(io_resume),
	ENA_STAT_GLOBAL_ENTRY(wd_expired),
	ENA_STAT_GLOBAL_ENTRY(interface_up),
	ENA_STAT_GLOBAL_ENTRY(interface_down),
	ENA_STAT_GLOBAL_ENTRY(admin_q_pause),
};

static const struct ena_stats ena_stats_tx_strings[] = {
	ENA_STAT_TX_ENTRY(cnt),
	ENA_STAT_TX_ENTRY(bytes),
	ENA_STAT_TX_ENTRY(queue_stop),
	ENA_STAT_TX_ENTRY(queue_wakeup),
	ENA_STAT_TX_ENTRY(dma_mapping_err),
	ENA_STAT_TX_ENTRY(linearize),
	ENA_STAT_TX_ENTRY(linearize_failed),
	ENA_STAT_TX_ENTRY(tx_poll),
	ENA_STAT_TX_ENTRY(doorbells),
	ENA_STAT_TX_ENTRY(prepare_ctx_err),
	ENA_STAT_TX_ENTRY(missing_tx_comp),
	ENA_STAT_TX_ENTRY(bad_req_id),
};

static const struct ena_stats ena_stats_rx_strings[] = {
	ENA_STAT_RX_ENTRY(cnt),
	ENA_STAT_RX_ENTRY(bytes),
	ENA_STAT_RX_ENTRY(refil_partial),
	ENA_STAT_RX_ENTRY(bad_csum),
	ENA_STAT_RX_ENTRY(page_alloc_fail),
	ENA_STAT_RX_ENTRY(skb_alloc_fail),
	ENA_STAT_RX_ENTRY(dma_mapping_err),
	ENA_STAT_RX_ENTRY(bad_desc_num),
	ENA_STAT_RX_ENTRY(small_copy_len_pkt),
};

static const struct ena_stats ena_stats_ena_com_strings[] = {
	ENA_STAT_ENA_COM_ENTRY(aborted_cmd),
	ENA_STAT_ENA_COM_ENTRY(submitted_cmd),
	ENA_STAT_ENA_COM_ENTRY(completed_cmd),
	ENA_STAT_ENA_COM_ENTRY(out_of_space),
	ENA_STAT_ENA_COM_ENTRY(no_completion),
};

#define ENA_STATS_ARRAY_GLOBAL	ARRAY_SIZE(ena_stats_global_strings)
#define ENA_STATS_ARRAY_TX	ARRAY_SIZE(ena_stats_tx_strings)
#define ENA_STATS_ARRAY_RX	ARRAY_SIZE(ena_stats_rx_strings)
#define ENA_STATS_ARRAY_ENA_COM	ARRAY_SIZE(ena_stats_ena_com_strings)

/** Vendor ID used by Amazon devices */
#define PCI_VENDOR_ID_AMAZON 0x1D0F
/** Amazon devices */
#define PCI_DEVICE_ID_ENA_VF	0xEC20
#define PCI_DEVICE_ID_ENA_LLQ_VF	0xEC21

#define	ENA_TX_OFFLOAD_MASK	(\
	PKT_TX_L4_MASK |         \
	PKT_TX_IP_CKSUM |        \
	PKT_TX_TCP_SEG)

#define	ENA_TX_OFFLOAD_NOTSUP_MASK	\
	(PKT_TX_OFFLOAD_MASK ^ ENA_TX_OFFLOAD_MASK)

static const struct rte_pci_id pci_id_ena_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AMAZON, PCI_DEVICE_ID_ENA_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AMAZON, PCI_DEVICE_ID_ENA_LLQ_VF) },
	{ .device_id = 0 },
};

static int ena_device_init(struct ena_com_dev *ena_dev,
			   struct ena_com_dev_get_features_ctx *get_feat_ctx);
static int ena_dev_configure(struct rte_eth_dev *dev);
static uint16_t eth_ena_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts);
static uint16_t eth_ena_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
static int ena_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			      uint16_t nb_desc, unsigned int socket_id,
			      const struct rte_eth_txconf *tx_conf);
static int ena_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			      uint16_t nb_desc, unsigned int socket_id,
			      const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mp);
static uint16_t eth_ena_recv_pkts(void *rx_queue,
				  struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
static int ena_populate_rx_queue(struct ena_ring *rxq, unsigned int count);
static void ena_init_rings(struct ena_adapter *adapter);
static int ena_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int ena_start(struct rte_eth_dev *dev);
static void ena_close(struct rte_eth_dev *dev);
static void ena_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
static void ena_rx_queue_release_all(struct rte_eth_dev *dev);
static void ena_tx_queue_release_all(struct rte_eth_dev *dev);
static void ena_rx_queue_release(void *queue);
static void ena_tx_queue_release(void *queue);
static void ena_rx_queue_release_bufs(struct ena_ring *ring);
static void ena_tx_queue_release_bufs(struct ena_ring *ring);
static int ena_link_update(struct rte_eth_dev *dev,
			   int wait_to_complete);
static int ena_queue_restart(struct ena_ring *ring);
static int ena_queue_restart_all(struct rte_eth_dev *dev,
				 enum ena_ring_type ring_type);
static void ena_stats_restart(struct rte_eth_dev *dev);
static void ena_infos_get(struct rte_eth_dev *dev,
			  struct rte_eth_dev_info *dev_info);
static int ena_rss_reta_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);
static int ena_rss_reta_query(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size);
static int ena_get_sset_count(struct rte_eth_dev *dev, int sset);

static const struct eth_dev_ops ena_dev_ops = {
	.dev_configure        = ena_dev_configure,
	.dev_infos_get        = ena_infos_get,
	.rx_queue_setup       = ena_rx_queue_setup,
	.tx_queue_setup       = ena_tx_queue_setup,
	.dev_start            = ena_start,
	.link_update          = ena_link_update,
	.stats_get            = ena_stats_get,
	.mtu_set              = ena_mtu_set,
	.rx_queue_release     = ena_rx_queue_release,
	.tx_queue_release     = ena_tx_queue_release,
	.dev_close            = ena_close,
	.reta_update          = ena_rss_reta_update,
	.reta_query           = ena_rss_reta_query,
};

#define NUMA_NO_NODE	SOCKET_ID_ANY

static inline int ena_cpu_to_node(int cpu)
{
	struct rte_config *config = rte_eal_get_configuration();

	if (likely(cpu < RTE_MAX_MEMZONE))
		return config->mem_config->memzone[cpu].socket_id;

	return NUMA_NO_NODE;
}

static inline void ena_rx_mbuf_prepare(struct rte_mbuf *mbuf,
				       struct ena_com_rx_ctx *ena_rx_ctx)
{
	uint64_t ol_flags = 0;

	if (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_TCP)
		ol_flags |= PKT_TX_TCP_CKSUM;
	else if (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_UDP)
		ol_flags |= PKT_TX_UDP_CKSUM;

	if (ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV4)
		ol_flags |= PKT_TX_IPV4;
	else if (ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV6)
		ol_flags |= PKT_TX_IPV6;

	if (unlikely(ena_rx_ctx->l4_csum_err))
		ol_flags |= PKT_RX_L4_CKSUM_BAD;
	if (unlikely(ena_rx_ctx->l3_csum_err))
		ol_flags |= PKT_RX_IP_CKSUM_BAD;

	mbuf->ol_flags = ol_flags;
}

static inline void ena_tx_mbuf_prepare(struct rte_mbuf *mbuf,
				       struct ena_com_tx_ctx *ena_tx_ctx)
{
	struct ena_com_tx_meta *ena_meta = &ena_tx_ctx->ena_meta;

	if (mbuf->ol_flags &
	    (PKT_TX_L4_MASK | PKT_TX_IP_CKSUM | PKT_TX_TCP_SEG)) {
		/* check if TSO is required */
		if (mbuf->ol_flags & PKT_TX_TCP_SEG) {
			ena_tx_ctx->tso_enable = true;

			ena_meta->l4_hdr_len = GET_L4_HDR_LEN(mbuf);
		}

		/* check if L3 checksum is needed */
		if (mbuf->ol_flags & PKT_TX_IP_CKSUM)
			ena_tx_ctx->l3_csum_enable = true;

		if (mbuf->ol_flags & PKT_TX_IPV6) {
			ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV6;
		} else {
			ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV4;

			/* set don't fragment (DF) flag */
			if (mbuf->packet_type &
				(RTE_PTYPE_L4_NONFRAG
				 | RTE_PTYPE_INNER_L4_NONFRAG))
				ena_tx_ctx->df = true;
		}

		/* check if L4 checksum is needed */
		switch (mbuf->ol_flags & PKT_TX_L4_MASK) {
		case PKT_TX_TCP_CKSUM:
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_TCP;
			ena_tx_ctx->l4_csum_enable = true;
			break;
		case PKT_TX_UDP_CKSUM:
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UDP;
			ena_tx_ctx->l4_csum_enable = true;
			break;
		default:
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UNKNOWN;
			ena_tx_ctx->l4_csum_enable = false;
			break;
		}

		ena_meta->mss = mbuf->tso_segsz;
		ena_meta->l3_hdr_len = mbuf->l3_len;
		ena_meta->l3_hdr_offset = mbuf->l2_len;
		/* this param needed only for TSO */
		ena_meta->l3_outer_hdr_len = 0;
		ena_meta->l3_outer_hdr_offset = 0;

		ena_tx_ctx->meta_valid = true;
	} else {
		ena_tx_ctx->meta_valid = false;
	}
}

static void ena_config_host_info(struct ena_com_dev *ena_dev)
{
	struct ena_admin_host_info *host_info;
	int rc;

	/* Allocate only the host info */
	rc = ena_com_allocate_host_info(ena_dev);
	if (rc) {
		RTE_LOG(ERR, PMD, "Cannot allocate host info\n");
		return;
	}

	host_info = ena_dev->host_attr.host_info;

	host_info->os_type = ENA_ADMIN_OS_DPDK;
	host_info->kernel_ver = RTE_VERSION;
	snprintf((char *)host_info->kernel_ver_str,
		 sizeof(host_info->kernel_ver_str),
		 "%s", rte_version());
	host_info->os_dist = RTE_VERSION;
	snprintf((char *)host_info->os_dist_str,
		 sizeof(host_info->os_dist_str),
		 "%s", rte_version());
	host_info->driver_version =
		(DRV_MODULE_VER_MAJOR) |
		(DRV_MODULE_VER_MINOR << ENA_ADMIN_HOST_INFO_MINOR_SHIFT) |
		(DRV_MODULE_VER_SUBMINOR <<
			ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT);

	rc = ena_com_set_host_attributes(ena_dev);
	if (rc) {
		RTE_LOG(ERR, PMD, "Cannot set host attributes\n");
		if (rc != -EPERM)
			goto err;
	}

	return;

err:
	ena_com_delete_host_info(ena_dev);
}

static int
ena_get_sset_count(struct rte_eth_dev *dev, int sset)
{
	if (sset != ETH_SS_STATS)
		return -EOPNOTSUPP;

	 /* Workaround for clang:
	 * touch internal structures to prevent
	 * compiler error
	 */
	ENA_TOUCH(ena_stats_global_strings);
	ENA_TOUCH(ena_stats_tx_strings);
	ENA_TOUCH(ena_stats_rx_strings);
	ENA_TOUCH(ena_stats_ena_com_strings);

	return  dev->data->nb_tx_queues *
		(ENA_STATS_ARRAY_TX + ENA_STATS_ARRAY_RX) +
		ENA_STATS_ARRAY_GLOBAL + ENA_STATS_ARRAY_ENA_COM;
}

static void ena_config_debug_area(struct ena_adapter *adapter)
{
	u32 debug_area_size;
	int rc, ss_count;

	ss_count = ena_get_sset_count(adapter->rte_dev, ETH_SS_STATS);
	if (ss_count <= 0) {
		RTE_LOG(ERR, PMD, "SS count is negative\n");
		return;
	}

	/* allocate 32 bytes for each string and 64bit for the value */
	debug_area_size = ss_count * ETH_GSTRING_LEN + sizeof(u64) * ss_count;

	rc = ena_com_allocate_debug_area(&adapter->ena_dev, debug_area_size);
	if (rc) {
		RTE_LOG(ERR, PMD, "Cannot allocate debug area\n");
		return;
	}

	rc = ena_com_set_host_attributes(&adapter->ena_dev);
	if (rc) {
		RTE_LOG(WARNING, PMD, "Cannot set host attributes\n");
		if (rc != -EPERM)
			goto err;
	}

	return;
err:
	ena_com_delete_debug_area(&adapter->ena_dev);
}

static void ena_close(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);

	adapter->state = ENA_ADAPTER_STATE_STOPPED;

	ena_rx_queue_release_all(dev);
	ena_tx_queue_release_all(dev);
}

static int ena_rss_reta_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int ret, i;
	u16 entry_value;
	int conf_idx;
	int idx;

	if ((reta_size == 0) || (reta_conf == NULL))
		return -EINVAL;

	if (reta_size > ENA_RX_RSS_TABLE_SIZE) {
		RTE_LOG(WARNING, PMD,
			"indirection table %d is bigger than supported (%d)\n",
			reta_size, ENA_RX_RSS_TABLE_SIZE);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0 ; i < reta_size ; i++) {
		/* each reta_conf is for 64 entries.
		 * to support 128 we use 2 conf of 64
		 */
		conf_idx = i / RTE_RETA_GROUP_SIZE;
		idx = i % RTE_RETA_GROUP_SIZE;
		if (TEST_BIT(reta_conf[conf_idx].mask, idx)) {
			entry_value =
				ENA_IO_RXQ_IDX(reta_conf[conf_idx].reta[idx]);
			ret = ena_com_indirect_table_fill_entry(ena_dev,
								i,
								entry_value);
			if (unlikely(ret && (ret != ENA_COM_PERMISSION))) {
				RTE_LOG(ERR, PMD,
					"Cannot fill indirect table\n");
				ret = -ENOTSUP;
				goto err;
			}
		}
	}

	ret = ena_com_indirect_table_set(ena_dev);
	if (unlikely(ret && (ret != ENA_COM_PERMISSION))) {
		RTE_LOG(ERR, PMD, "Cannot flush the indirect table\n");
		ret = -ENOTSUP;
		goto err;
	}

	RTE_LOG(DEBUG, PMD, "%s(): RSS configured %d entries  for port %d\n",
		__func__, reta_size, adapter->rte_dev->data->port_id);
err:
	return ret;
}

/* Query redirection table. */
static int ena_rss_reta_query(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int ret;
	int i;
	u32 indirect_table[ENA_RX_RSS_TABLE_SIZE] = {0};
	int reta_conf_idx;
	int reta_idx;

	if (reta_size == 0 || reta_conf == NULL ||
	    (reta_size > RTE_RETA_GROUP_SIZE && ((reta_conf + 1) == NULL)))
		return -EINVAL;

	ret = ena_com_indirect_table_get(ena_dev, indirect_table);
	if (unlikely(ret && (ret != ENA_COM_PERMISSION))) {
		RTE_LOG(ERR, PMD, "cannot get indirect table\n");
		ret = -ENOTSUP;
		goto err;
	}

	for (i = 0 ; i < reta_size ; i++) {
		reta_conf_idx = i / RTE_RETA_GROUP_SIZE;
		reta_idx = i % RTE_RETA_GROUP_SIZE;
		if (TEST_BIT(reta_conf[reta_conf_idx].mask, reta_idx))
			reta_conf[reta_conf_idx].reta[reta_idx] =
				ENA_IO_RXQ_IDX_REV(indirect_table[i]);
	}
err:
	return ret;
}

static int ena_rss_init_default(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	uint16_t nb_rx_queues = adapter->rte_dev->data->nb_rx_queues;
	int rc, i;
	u32 val;

	rc = ena_com_rss_init(ena_dev, ENA_RX_RSS_TABLE_LOG_SIZE);
	if (unlikely(rc)) {
		RTE_LOG(ERR, PMD, "Cannot init indirect table\n");
		goto err_rss_init;
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; i++) {
		val = i % nb_rx_queues;
		rc = ena_com_indirect_table_fill_entry(ena_dev, i,
						       ENA_IO_RXQ_IDX(val));
		if (unlikely(rc && (rc != ENA_COM_PERMISSION))) {
			RTE_LOG(ERR, PMD, "Cannot fill indirect table\n");
			goto err_fill_indir;
		}
	}

	rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_CRC32, NULL,
					ENA_HASH_KEY_SIZE, 0xFFFFFFFF);
	if (unlikely(rc && (rc != ENA_COM_PERMISSION))) {
		RTE_LOG(INFO, PMD, "Cannot fill hash function\n");
		goto err_fill_indir;
	}

	rc = ena_com_set_default_hash_ctrl(ena_dev);
	if (unlikely(rc && (rc != ENA_COM_PERMISSION))) {
		RTE_LOG(INFO, PMD, "Cannot fill hash control\n");
		goto err_fill_indir;
	}

	rc = ena_com_indirect_table_set(ena_dev);
	if (unlikely(rc && (rc != ENA_COM_PERMISSION))) {
		RTE_LOG(ERR, PMD, "Cannot flush the indirect table\n");
		goto err_fill_indir;
	}
	RTE_LOG(DEBUG, PMD, "RSS configured for port %d\n",
		adapter->rte_dev->data->port_id);

	return 0;

err_fill_indir:
	ena_com_rss_destroy(ena_dev);
err_rss_init:

	return rc;
}

static void ena_rx_queue_release_all(struct rte_eth_dev *dev)
{
	struct ena_ring **queues = (struct ena_ring **)dev->data->rx_queues;
	int nb_queues = dev->data->nb_rx_queues;
	int i;

	for (i = 0; i < nb_queues; i++)
		ena_rx_queue_release(queues[i]);
}

static void ena_tx_queue_release_all(struct rte_eth_dev *dev)
{
	struct ena_ring **queues = (struct ena_ring **)dev->data->tx_queues;
	int nb_queues = dev->data->nb_tx_queues;
	int i;

	for (i = 0; i < nb_queues; i++)
		ena_tx_queue_release(queues[i]);
}

static void ena_rx_queue_release(void *queue)
{
	struct ena_ring *ring = (struct ena_ring *)queue;
	struct ena_adapter *adapter = ring->adapter;
	int ena_qid;

	ena_assert_msg(ring->configured,
		       "API violation - releasing not configured queue");
	ena_assert_msg(ring->adapter->state != ENA_ADAPTER_STATE_RUNNING,
		       "API violation");

	/* Destroy HW queue */
	ena_qid = ENA_IO_RXQ_IDX(ring->id);
	ena_com_destroy_io_queue(&adapter->ena_dev, ena_qid);

	/* Free all bufs */
	ena_rx_queue_release_bufs(ring);

	/* Free ring resources */
	if (ring->rx_buffer_info)
		rte_free(ring->rx_buffer_info);
	ring->rx_buffer_info = NULL;

	ring->configured = 0;

	RTE_LOG(NOTICE, PMD, "RX Queue %d:%d released\n",
		ring->port_id, ring->id);
}

static void ena_tx_queue_release(void *queue)
{
	struct ena_ring *ring = (struct ena_ring *)queue;
	struct ena_adapter *adapter = ring->adapter;
	int ena_qid;

	ena_assert_msg(ring->configured,
		       "API violation. Releasing not configured queue");
	ena_assert_msg(ring->adapter->state != ENA_ADAPTER_STATE_RUNNING,
		       "API violation");

	/* Destroy HW queue */
	ena_qid = ENA_IO_TXQ_IDX(ring->id);
	ena_com_destroy_io_queue(&adapter->ena_dev, ena_qid);

	/* Free all bufs */
	ena_tx_queue_release_bufs(ring);

	/* Free ring resources */
	if (ring->tx_buffer_info)
		rte_free(ring->tx_buffer_info);

	if (ring->empty_tx_reqs)
		rte_free(ring->empty_tx_reqs);

	ring->empty_tx_reqs = NULL;
	ring->tx_buffer_info = NULL;

	ring->configured = 0;

	RTE_LOG(NOTICE, PMD, "TX Queue %d:%d released\n",
		ring->port_id, ring->id);
}

static void ena_rx_queue_release_bufs(struct ena_ring *ring)
{
	unsigned int ring_mask = ring->ring_size - 1;

	while (ring->next_to_clean != ring->next_to_use) {
		struct rte_mbuf *m =
			ring->rx_buffer_info[ring->next_to_clean & ring_mask];

		if (m)
			rte_mbuf_raw_free(m);

		ring->next_to_clean++;
	}
}

static void ena_tx_queue_release_bufs(struct ena_ring *ring)
{
	unsigned int ring_mask = ring->ring_size - 1;

	while (ring->next_to_clean != ring->next_to_use) {
		struct ena_tx_buffer *tx_buf =
			&ring->tx_buffer_info[ring->next_to_clean & ring_mask];

		if (tx_buf->mbuf)
			rte_pktmbuf_free(tx_buf->mbuf);

		ring->next_to_clean++;
	}
}

static int ena_link_update(struct rte_eth_dev *dev,
			   __rte_unused int wait_to_complete)
{
	struct rte_eth_link *link = &dev->data->dev_link;

	link->link_status = 1;
	link->link_speed = ETH_SPEED_NUM_10G;
	link->link_duplex = ETH_LINK_FULL_DUPLEX;

	return 0;
}

static int ena_queue_restart_all(struct rte_eth_dev *dev,
				 enum ena_ring_type ring_type)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	struct ena_ring *queues = NULL;
	int i = 0;
	int rc = 0;

	queues = (ring_type == ENA_RING_TYPE_RX) ?
		adapter->rx_ring : adapter->tx_ring;

	for (i = 0; i < adapter->num_queues; i++) {
		if (queues[i].configured) {
			if (ring_type == ENA_RING_TYPE_RX) {
				ena_assert_msg(
					dev->data->rx_queues[i] == &queues[i],
					"Inconsistent state of rx queues\n");
			} else {
				ena_assert_msg(
					dev->data->tx_queues[i] == &queues[i],
					"Inconsistent state of tx queues\n");
			}

			rc = ena_queue_restart(&queues[i]);

			if (rc) {
				PMD_INIT_LOG(ERR,
					     "failed to restart queue %d type(%d)",
					     i, ring_type);
				return -1;
			}
		}
	}

	return 0;
}

static uint32_t ena_get_mtu_conf(struct ena_adapter *adapter)
{
	uint32_t max_frame_len = adapter->max_mtu;

	if (adapter->rte_eth_dev_data->dev_conf.rxmode.jumbo_frame == 1)
		max_frame_len =
			adapter->rte_eth_dev_data->dev_conf.rxmode.max_rx_pkt_len;

	return max_frame_len;
}

static int ena_check_valid_conf(struct ena_adapter *adapter)
{
	uint32_t max_frame_len = ena_get_mtu_conf(adapter);

	if (max_frame_len > adapter->max_mtu) {
		PMD_INIT_LOG(ERR, "Unsupported MTU of %d", max_frame_len);
		return -1;
	}

	return 0;
}

static int
ena_calc_queue_size(struct ena_com_dev *ena_dev,
		    struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	uint32_t queue_size = ENA_DEFAULT_RING_SIZE;

	queue_size = RTE_MIN(queue_size,
			     get_feat_ctx->max_queues.max_cq_depth);
	queue_size = RTE_MIN(queue_size,
			     get_feat_ctx->max_queues.max_sq_depth);

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV)
		queue_size = RTE_MIN(queue_size,
				     get_feat_ctx->max_queues.max_llq_depth);

	/* Round down to power of 2 */
	if (!rte_is_power_of_2(queue_size))
		queue_size = rte_align32pow2(queue_size >> 1);

	if (queue_size == 0) {
		PMD_INIT_LOG(ERR, "Invalid queue size");
		return -EFAULT;
	}

	return queue_size;
}

static void ena_stats_restart(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);

	rte_atomic64_init(&adapter->drv_stats->ierrors);
	rte_atomic64_init(&adapter->drv_stats->oerrors);
	rte_atomic64_init(&adapter->drv_stats->rx_nombuf);
}

static void ena_stats_get(struct rte_eth_dev *dev,
			  struct rte_eth_stats *stats)
{
	struct ena_admin_basic_stats ena_stats;
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	memset(&ena_stats, 0, sizeof(ena_stats));
	rc = ena_com_get_dev_basic_stats(ena_dev, &ena_stats);
	if (unlikely(rc)) {
		RTE_LOG(ERR, PMD, "Could not retrieve statistics from ENA");
		return;
	}

	/* Set of basic statistics from ENA */
	stats->ipackets = __MERGE_64B_H_L(ena_stats.rx_pkts_high,
					  ena_stats.rx_pkts_low);
	stats->opackets = __MERGE_64B_H_L(ena_stats.tx_pkts_high,
					  ena_stats.tx_pkts_low);
	stats->ibytes = __MERGE_64B_H_L(ena_stats.rx_bytes_high,
					ena_stats.rx_bytes_low);
	stats->obytes = __MERGE_64B_H_L(ena_stats.tx_bytes_high,
					ena_stats.tx_bytes_low);
	stats->imissed = __MERGE_64B_H_L(ena_stats.rx_drops_high,
					 ena_stats.rx_drops_low);

	/* Driver related stats */
	stats->ierrors = rte_atomic64_read(&adapter->drv_stats->ierrors);
	stats->oerrors = rte_atomic64_read(&adapter->drv_stats->oerrors);
	stats->rx_nombuf = rte_atomic64_read(&adapter->drv_stats->rx_nombuf);
}

static int ena_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev;
	int rc = 0;

	ena_assert_msg(dev->data != NULL, "Uninitialized device");
	ena_assert_msg(dev->data->dev_private != NULL, "Uninitialized device");
	adapter = (struct ena_adapter *)(dev->data->dev_private);

	ena_dev = &adapter->ena_dev;
	ena_assert_msg(ena_dev != NULL, "Uninitialized device");

	if (mtu > ena_get_mtu_conf(adapter)) {
		RTE_LOG(ERR, PMD,
			"Given MTU (%d) exceeds maximum MTU supported (%d)\n",
			mtu, ena_get_mtu_conf(adapter));
		rc = -EINVAL;
		goto err;
	}

	rc = ena_com_set_dev_mtu(ena_dev, mtu);
	if (rc)
		RTE_LOG(ERR, PMD, "Could not set MTU: %d\n", mtu);
	else
		RTE_LOG(NOTICE, PMD, "Set MTU: %d\n", mtu);

err:
	return rc;
}

static int ena_start(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	int rc = 0;

	if (!(adapter->state == ENA_ADAPTER_STATE_CONFIG ||
	      adapter->state == ENA_ADAPTER_STATE_STOPPED)) {
		PMD_INIT_LOG(ERR, "API violation");
		return -1;
	}

	rc = ena_check_valid_conf(adapter);
	if (rc)
		return rc;

	rc = ena_queue_restart_all(dev, ENA_RING_TYPE_RX);
	if (rc)
		return rc;

	rc = ena_queue_restart_all(dev, ENA_RING_TYPE_TX);
	if (rc)
		return rc;

	if (adapter->rte_dev->data->dev_conf.rxmode.mq_mode &
	    ETH_MQ_RX_RSS_FLAG) {
		rc = ena_rss_init_default(adapter);
		if (rc)
			return rc;
	}

	ena_stats_restart(dev);

	adapter->state = ENA_ADAPTER_STATE_RUNNING;

	return 0;
}

static int ena_queue_restart(struct ena_ring *ring)
{
	int rc, bufs_num;

	ena_assert_msg(ring->configured == 1,
		       "Trying to restart unconfigured queue\n");

	ring->next_to_clean = 0;
	ring->next_to_use = 0;

	if (ring->type == ENA_RING_TYPE_TX)
		return 0;

	bufs_num = ring->ring_size - 1;
	rc = ena_populate_rx_queue(ring, bufs_num);
	if (rc != bufs_num) {
		PMD_INIT_LOG(ERR, "Failed to populate rx ring !");
		return (-1);
	}

	return 0;
}

static int ena_tx_queue_setup(struct rte_eth_dev *dev,
			      uint16_t queue_idx,
			      uint16_t nb_desc,
			      __rte_unused unsigned int socket_id,
			      __rte_unused const struct rte_eth_txconf *tx_conf)
{
	struct ena_com_create_io_ctx ctx =
		/* policy set to _HOST just to satisfy icc compiler */
		{ ENA_ADMIN_PLACEMENT_POLICY_HOST,
		  ENA_COM_IO_QUEUE_DIRECTION_TX, 0, 0, 0, 0 };
	struct ena_ring *txq = NULL;
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	unsigned int i;
	int ena_qid;
	int rc;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;

	txq = &adapter->tx_ring[queue_idx];

	if (txq->configured) {
		RTE_LOG(CRIT, PMD,
			"API violation. Queue %d is already configured\n",
			queue_idx);
		return -1;
	}

	if (!rte_is_power_of_2(nb_desc)) {
		RTE_LOG(ERR, PMD,
			"Unsupported size of RX queue: %d is not a power of 2.",
			nb_desc);
		return -EINVAL;
	}

	if (nb_desc > adapter->tx_ring_size) {
		RTE_LOG(ERR, PMD,
			"Unsupported size of TX queue (max size: %d)\n",
			adapter->tx_ring_size);
		return -EINVAL;
	}

	ena_qid = ENA_IO_TXQ_IDX(queue_idx);

	ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_TX;
	ctx.qid = ena_qid;
	ctx.msix_vector = -1; /* admin interrupts not used */
	ctx.mem_queue_type = ena_dev->tx_mem_queue_type;
	ctx.queue_size = adapter->tx_ring_size;
	ctx.numa_node = ena_cpu_to_node(queue_idx);

	rc = ena_com_create_io_queue(ena_dev, &ctx);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"failed to create io TX queue #%d (qid:%d) rc: %d\n",
			queue_idx, ena_qid, rc);
	}
	txq->ena_com_io_cq = &ena_dev->io_cq_queues[ena_qid];
	txq->ena_com_io_sq = &ena_dev->io_sq_queues[ena_qid];

	rc = ena_com_get_io_handlers(ena_dev, ena_qid,
				     &txq->ena_com_io_sq,
				     &txq->ena_com_io_cq);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Failed to get TX queue handlers. TX queue num %d rc: %d\n",
			queue_idx, rc);
		ena_com_destroy_io_queue(ena_dev, ena_qid);
		goto err;
	}

	txq->port_id = dev->data->port_id;
	txq->next_to_clean = 0;
	txq->next_to_use = 0;
	txq->ring_size = nb_desc;

	txq->tx_buffer_info = rte_zmalloc("txq->tx_buffer_info",
					  sizeof(struct ena_tx_buffer) *
					  txq->ring_size,
					  RTE_CACHE_LINE_SIZE);
	if (!txq->tx_buffer_info) {
		RTE_LOG(ERR, PMD, "failed to alloc mem for tx buffer info\n");
		return -ENOMEM;
	}

	txq->empty_tx_reqs = rte_zmalloc("txq->empty_tx_reqs",
					 sizeof(u16) * txq->ring_size,
					 RTE_CACHE_LINE_SIZE);
	if (!txq->empty_tx_reqs) {
		RTE_LOG(ERR, PMD, "failed to alloc mem for tx reqs\n");
		rte_free(txq->tx_buffer_info);
		return -ENOMEM;
	}
	for (i = 0; i < txq->ring_size; i++)
		txq->empty_tx_reqs[i] = i;

	/* Store pointer to this queue in upper layer */
	txq->configured = 1;
	dev->data->tx_queues[queue_idx] = txq;
err:
	return rc;
}

static int ena_rx_queue_setup(struct rte_eth_dev *dev,
			      uint16_t queue_idx,
			      uint16_t nb_desc,
			      __rte_unused unsigned int socket_id,
			      __rte_unused const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mp)
{
	struct ena_com_create_io_ctx ctx =
		/* policy set to _HOST just to satisfy icc compiler */
		{ ENA_ADMIN_PLACEMENT_POLICY_HOST,
		  ENA_COM_IO_QUEUE_DIRECTION_RX, 0, 0, 0, 0 };
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);
	struct ena_ring *rxq = NULL;
	uint16_t ena_qid = 0;
	int rc = 0;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;

	rxq = &adapter->rx_ring[queue_idx];
	if (rxq->configured) {
		RTE_LOG(CRIT, PMD,
			"API violation. Queue %d is already configured\n",
			queue_idx);
		return -1;
	}

	if (!rte_is_power_of_2(nb_desc)) {
		RTE_LOG(ERR, PMD,
			"Unsupported size of TX queue: %d is not a power of 2.",
			nb_desc);
		return -EINVAL;
	}

	if (nb_desc > adapter->rx_ring_size) {
		RTE_LOG(ERR, PMD,
			"Unsupported size of RX queue (max size: %d)\n",
			adapter->rx_ring_size);
		return -EINVAL;
	}

	ena_qid = ENA_IO_RXQ_IDX(queue_idx);

	ctx.qid = ena_qid;
	ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_RX;
	ctx.mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
	ctx.msix_vector = -1; /* admin interrupts not used */
	ctx.queue_size = adapter->rx_ring_size;
	ctx.numa_node = ena_cpu_to_node(queue_idx);

	rc = ena_com_create_io_queue(ena_dev, &ctx);
	if (rc)
		RTE_LOG(ERR, PMD, "failed to create io RX queue #%d rc: %d\n",
			queue_idx, rc);

	rxq->ena_com_io_cq = &ena_dev->io_cq_queues[ena_qid];
	rxq->ena_com_io_sq = &ena_dev->io_sq_queues[ena_qid];

	rc = ena_com_get_io_handlers(ena_dev, ena_qid,
				     &rxq->ena_com_io_sq,
				     &rxq->ena_com_io_cq);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Failed to get RX queue handlers. RX queue num %d rc: %d\n",
			queue_idx, rc);
		ena_com_destroy_io_queue(ena_dev, ena_qid);
	}

	rxq->port_id = dev->data->port_id;
	rxq->next_to_clean = 0;
	rxq->next_to_use = 0;
	rxq->ring_size = nb_desc;
	rxq->mb_pool = mp;

	rxq->rx_buffer_info = rte_zmalloc("rxq->buffer_info",
					  sizeof(struct rte_mbuf *) * nb_desc,
					  RTE_CACHE_LINE_SIZE);
	if (!rxq->rx_buffer_info) {
		RTE_LOG(ERR, PMD, "failed to alloc mem for rx buffer info\n");
		return -ENOMEM;
	}

	/* Store pointer to this queue in upper layer */
	rxq->configured = 1;
	dev->data->rx_queues[queue_idx] = rxq;

	return rc;
}

static int ena_populate_rx_queue(struct ena_ring *rxq, unsigned int count)
{
	unsigned int i;
	int rc;
	uint16_t ring_size = rxq->ring_size;
	uint16_t ring_mask = ring_size - 1;
	uint16_t next_to_use = rxq->next_to_use;
	uint16_t in_use;
	struct rte_mbuf **mbufs = &rxq->rx_buffer_info[0];

	if (unlikely(!count))
		return 0;

	in_use = rxq->next_to_use - rxq->next_to_clean;
	ena_assert_msg(((in_use + count) < ring_size), "bad ring state");

	count = RTE_MIN(count,
			(uint16_t)(ring_size - (next_to_use & ring_mask)));

	/* get resources for incoming packets */
	rc = rte_mempool_get_bulk(rxq->mb_pool,
				  (void **)(&mbufs[next_to_use & ring_mask]),
				  count);
	if (unlikely(rc < 0)) {
		rte_atomic64_inc(&rxq->adapter->drv_stats->rx_nombuf);
		PMD_RX_LOG(DEBUG, "there are no enough free buffers");
		return 0;
	}

	for (i = 0; i < count; i++) {
		uint16_t next_to_use_masked = next_to_use & ring_mask;
		struct rte_mbuf *mbuf = mbufs[next_to_use_masked];
		struct ena_com_buf ebuf;

		rte_prefetch0(mbufs[((next_to_use + 4) & ring_mask)]);
		/* prepare physical address for DMA transaction */
		ebuf.paddr = mbuf->buf_physaddr + RTE_PKTMBUF_HEADROOM;
		ebuf.len = mbuf->buf_len - RTE_PKTMBUF_HEADROOM;
		/* pass resource to device */
		rc = ena_com_add_single_rx_desc(rxq->ena_com_io_sq,
						&ebuf, next_to_use_masked);
		if (unlikely(rc)) {
			rte_mempool_put_bulk(rxq->mb_pool, (void **)(&mbuf),
					     count - i);
			RTE_LOG(WARNING, PMD, "failed adding rx desc\n");
			break;
		}
		next_to_use++;
	}

	/* When we submitted free recources to device... */
	if (i > 0) {
		/* ...let HW know that it can fill buffers with data */
		rte_wmb();
		ena_com_write_sq_doorbell(rxq->ena_com_io_sq);

		rxq->next_to_use = next_to_use;
	}

	return i;
}

static int ena_device_init(struct ena_com_dev *ena_dev,
			   struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	int rc;
	bool readless_supported;

	/* Initialize mmio registers */
	rc = ena_com_mmio_reg_read_request_init(ena_dev);
	if (rc) {
		RTE_LOG(ERR, PMD, "failed to init mmio read less\n");
		return rc;
	}

	/* The PCIe configuration space revision id indicate if mmio reg
	 * read is disabled.
	 */
	readless_supported =
		!(((struct rte_pci_device *)ena_dev->dmadev)->id.class_id
			       & ENA_MMIO_DISABLE_REG_READ);
	ena_com_set_mmio_read_mode(ena_dev, readless_supported);

	/* reset device */
	rc = ena_com_dev_reset(ena_dev);
	if (rc) {
		RTE_LOG(ERR, PMD, "cannot reset device\n");
		goto err_mmio_read_less;
	}

	/* check FW version */
	rc = ena_com_validate_version(ena_dev);
	if (rc) {
		RTE_LOG(ERR, PMD, "device version is too low\n");
		goto err_mmio_read_less;
	}

	ena_dev->dma_addr_bits = ena_com_get_dma_width(ena_dev);

	/* ENA device administration layer init */
	rc = ena_com_admin_init(ena_dev, NULL, true);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"cannot initialize ena admin queue with device\n");
		goto err_mmio_read_less;
	}

	/* To enable the msix interrupts the driver needs to know the number
	 * of queues. So the driver uses polling mode to retrieve this
	 * information.
	 */
	ena_com_set_admin_polling_mode(ena_dev, true);

	ena_config_host_info(ena_dev);

	/* Get Device Attributes and features */
	rc = ena_com_get_dev_attr_feat(ena_dev, get_feat_ctx);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"cannot get attribute for ena device rc= %d\n", rc);
		goto err_admin_init;
	}

	return 0;

err_admin_init:
	ena_com_admin_destroy(ena_dev);

err_mmio_read_less:
	ena_com_mmio_reg_read_request_destroy(ena_dev);

	return rc;
}

static int eth_ena_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct ena_adapter *adapter =
		(struct ena_adapter *)(eth_dev->data->dev_private);
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	struct ena_com_dev_get_features_ctx get_feat_ctx;
	int queue_size, rc;

	static int adapters_found;

	memset(adapter, 0, sizeof(struct ena_adapter));
	ena_dev = &adapter->ena_dev;

	eth_dev->dev_ops = &ena_dev_ops;
	eth_dev->rx_pkt_burst = &eth_ena_recv_pkts;
	eth_dev->tx_pkt_burst = &eth_ena_xmit_pkts;
	eth_dev->tx_pkt_prepare = &eth_ena_prep_pkts;
	adapter->rte_eth_dev_data = eth_dev->data;
	adapter->rte_dev = eth_dev;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	adapter->pdev = pci_dev;

	PMD_INIT_LOG(INFO, "Initializing %x:%x:%x.%d",
		     pci_dev->addr.domain,
		     pci_dev->addr.bus,
		     pci_dev->addr.devid,
		     pci_dev->addr.function);

	adapter->regs = pci_dev->mem_resource[ENA_REGS_BAR].addr;
	adapter->dev_mem_base = pci_dev->mem_resource[ENA_MEM_BAR].addr;

	/* Present ENA_MEM_BAR indicates available LLQ mode.
	 * Use corresponding policy
	 */
	if (adapter->dev_mem_base)
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_DEV;
	else if (adapter->regs)
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
	else
		PMD_INIT_LOG(CRIT, "Failed to access registers BAR(%d)",
			     ENA_REGS_BAR);

	ena_dev->reg_bar = adapter->regs;
	ena_dev->dmadev = adapter->pdev;

	adapter->id_number = adapters_found;

	snprintf(adapter->name, ENA_NAME_MAX_LEN, "ena_%d",
		 adapter->id_number);

	/* device specific initialization routine */
	rc = ena_device_init(ena_dev, &get_feat_ctx);
	if (rc) {
		PMD_INIT_LOG(CRIT, "Failed to init ENA device");
		return -1;
	}

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV) {
		if (get_feat_ctx.max_queues.max_llq_num == 0) {
			PMD_INIT_LOG(ERR,
				     "Trying to use LLQ but llq_num is 0.\n"
				     "Fall back into regular queues.");
			ena_dev->tx_mem_queue_type =
				ENA_ADMIN_PLACEMENT_POLICY_HOST;
			adapter->num_queues =
				get_feat_ctx.max_queues.max_sq_num;
		} else {
			adapter->num_queues =
				get_feat_ctx.max_queues.max_llq_num;
		}
	} else {
		adapter->num_queues = get_feat_ctx.max_queues.max_sq_num;
	}

	queue_size = ena_calc_queue_size(ena_dev, &get_feat_ctx);
	if ((queue_size <= 0) || (adapter->num_queues <= 0))
		return -EFAULT;

	adapter->tx_ring_size = queue_size;
	adapter->rx_ring_size = queue_size;

	/* prepare ring structures */
	ena_init_rings(adapter);

	ena_config_debug_area(adapter);

	/* Set max MTU for this device */
	adapter->max_mtu = get_feat_ctx.dev_attr.max_mtu;

	/* set device support for TSO */
	adapter->tso4_supported = get_feat_ctx.offload.tx &
				  ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK;

	/* Copy MAC address and point DPDK to it */
	eth_dev->data->mac_addrs = (struct ether_addr *)adapter->mac_addr;
	ether_addr_copy((struct ether_addr *)get_feat_ctx.dev_attr.mac_addr,
			(struct ether_addr *)adapter->mac_addr);

	adapter->drv_stats = rte_zmalloc("adapter stats",
					 sizeof(*adapter->drv_stats),
					 RTE_CACHE_LINE_SIZE);
	if (!adapter->drv_stats) {
		RTE_LOG(ERR, PMD, "failed to alloc mem for adapter stats\n");
		return -ENOMEM;
	}

	adapters_found++;
	adapter->state = ENA_ADAPTER_STATE_INIT;

	return 0;
}

static int ena_dev_configure(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter =
		(struct ena_adapter *)(dev->data->dev_private);

	if (!(adapter->state == ENA_ADAPTER_STATE_INIT ||
	      adapter->state == ENA_ADAPTER_STATE_STOPPED)) {
		PMD_INIT_LOG(ERR, "Illegal adapter state: %d",
			     adapter->state);
		return -1;
	}

	switch (adapter->state) {
	case ENA_ADAPTER_STATE_INIT:
	case ENA_ADAPTER_STATE_STOPPED:
		adapter->state = ENA_ADAPTER_STATE_CONFIG;
		break;
	case ENA_ADAPTER_STATE_CONFIG:
		RTE_LOG(WARNING, PMD,
			"Ivalid driver state while trying to configure device\n");
		break;
	default:
		break;
	}

	return 0;
}

static void ena_init_rings(struct ena_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_queues; i++) {
		struct ena_ring *ring = &adapter->tx_ring[i];

		ring->configured = 0;
		ring->type = ENA_RING_TYPE_TX;
		ring->adapter = adapter;
		ring->id = i;
		ring->tx_mem_queue_type = adapter->ena_dev.tx_mem_queue_type;
		ring->tx_max_header_size = adapter->ena_dev.tx_max_header_size;
	}

	for (i = 0; i < adapter->num_queues; i++) {
		struct ena_ring *ring = &adapter->rx_ring[i];

		ring->configured = 0;
		ring->type = ENA_RING_TYPE_RX;
		ring->adapter = adapter;
		ring->id = i;
	}
}

static void ena_infos_get(struct rte_eth_dev *dev,
			  struct rte_eth_dev_info *dev_info)
{
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev;
	struct ena_com_dev_get_features_ctx feat;
	uint32_t rx_feat = 0, tx_feat = 0;
	int rc = 0;

	ena_assert_msg(dev->data != NULL, "Uninitialized device");
	ena_assert_msg(dev->data->dev_private != NULL, "Uninitialized device");
	adapter = (struct ena_adapter *)(dev->data->dev_private);

	ena_dev = &adapter->ena_dev;
	ena_assert_msg(ena_dev != NULL, "Uninitialized device");

	dev_info->pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	dev_info->speed_capa =
			ETH_LINK_SPEED_1G   |
			ETH_LINK_SPEED_2_5G |
			ETH_LINK_SPEED_5G   |
			ETH_LINK_SPEED_10G  |
			ETH_LINK_SPEED_25G  |
			ETH_LINK_SPEED_40G  |
			ETH_LINK_SPEED_50G  |
			ETH_LINK_SPEED_100G;

	/* Get supported features from HW */
	rc = ena_com_get_dev_attr_feat(ena_dev, &feat);
	if (unlikely(rc)) {
		RTE_LOG(ERR, PMD,
			"Cannot get attribute for ena device rc= %d\n", rc);
		return;
	}

	/* Set Tx & Rx features available for device */
	if (feat.offload.tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK)
		tx_feat	|= DEV_TX_OFFLOAD_TCP_TSO;

	if (feat.offload.tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK)
		tx_feat |= DEV_TX_OFFLOAD_IPV4_CKSUM |
			DEV_TX_OFFLOAD_UDP_CKSUM |
			DEV_TX_OFFLOAD_TCP_CKSUM;

	if (feat.offload.rx_supported &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK)
		rx_feat |= DEV_RX_OFFLOAD_IPV4_CKSUM |
			DEV_RX_OFFLOAD_UDP_CKSUM  |
			DEV_RX_OFFLOAD_TCP_CKSUM;

	/* Inform framework about available features */
	dev_info->rx_offload_capa = rx_feat;
	dev_info->tx_offload_capa = tx_feat;

	dev_info->min_rx_bufsize = ENA_MIN_FRAME_LEN;
	dev_info->max_rx_pktlen  = adapter->max_mtu;
	dev_info->max_mac_addrs = 1;

	dev_info->max_rx_queues = adapter->num_queues;
	dev_info->max_tx_queues = adapter->num_queues;
	dev_info->reta_size = ENA_RX_RSS_TABLE_SIZE;
}

static uint16_t eth_ena_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	struct ena_ring *rx_ring = (struct ena_ring *)(rx_queue);
	unsigned int ring_size = rx_ring->ring_size;
	unsigned int ring_mask = ring_size - 1;
	uint16_t next_to_clean = rx_ring->next_to_clean;
	uint16_t desc_in_use = 0;
	unsigned int recv_idx = 0;
	struct rte_mbuf *mbuf = NULL;
	struct rte_mbuf *mbuf_head = NULL;
	struct rte_mbuf *mbuf_prev = NULL;
	struct rte_mbuf **rx_buff_info = rx_ring->rx_buffer_info;
	unsigned int completed;

	struct ena_com_rx_ctx ena_rx_ctx;
	int rc = 0;

	/* Check adapter state */
	if (unlikely(rx_ring->adapter->state != ENA_ADAPTER_STATE_RUNNING)) {
		RTE_LOG(ALERT, PMD,
			"Trying to receive pkts while device is NOT running\n");
		return 0;
	}

	desc_in_use = rx_ring->next_to_use - next_to_clean;
	if (unlikely(nb_pkts > desc_in_use))
		nb_pkts = desc_in_use;

	for (completed = 0; completed < nb_pkts; completed++) {
		int segments = 0;

		ena_rx_ctx.max_bufs = rx_ring->ring_size;
		ena_rx_ctx.ena_bufs = rx_ring->ena_bufs;
		ena_rx_ctx.descs = 0;
		/* receive packet context */
		rc = ena_com_rx_pkt(rx_ring->ena_com_io_cq,
				    rx_ring->ena_com_io_sq,
				    &ena_rx_ctx);
		if (unlikely(rc)) {
			RTE_LOG(ERR, PMD, "ena_com_rx_pkt error %d\n", rc);
			return 0;
		}

		if (unlikely(ena_rx_ctx.descs == 0))
			break;

		while (segments < ena_rx_ctx.descs) {
			mbuf = rx_buff_info[next_to_clean & ring_mask];
			mbuf->data_len = ena_rx_ctx.ena_bufs[segments].len;
			mbuf->data_off = RTE_PKTMBUF_HEADROOM;
			mbuf->refcnt = 1;
			mbuf->next = NULL;
			if (segments == 0) {
				mbuf->nb_segs = ena_rx_ctx.descs;
				mbuf->port = rx_ring->port_id;
				mbuf->pkt_len = 0;
				mbuf_head = mbuf;
			} else {
				/* for multi-segment pkts create mbuf chain */
				mbuf_prev->next = mbuf;
			}
			mbuf_head->pkt_len += mbuf->data_len;

			mbuf_prev = mbuf;
			segments++;
			next_to_clean++;
		}

		/* fill mbuf attributes if any */
		ena_rx_mbuf_prepare(mbuf_head, &ena_rx_ctx);
		mbuf_head->hash.rss = (uint32_t)rx_ring->id;

		/* pass to DPDK application head mbuf */
		rx_pkts[recv_idx] = mbuf_head;
		recv_idx++;
	}

	rx_ring->next_to_clean = next_to_clean;

	desc_in_use = desc_in_use - completed + 1;
	/* Burst refill to save doorbells, memory barriers, const interval */
	if (ring_size - desc_in_use > ENA_RING_DESCS_RATIO(ring_size))
		ena_populate_rx_queue(rx_ring, ring_size - desc_in_use);

	return recv_idx;
}

static uint16_t
eth_ena_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int32_t ret;
	uint32_t i;
	struct rte_mbuf *m;
	struct ena_ring *tx_ring = (struct ena_ring *)(tx_queue);
	struct ipv4_hdr *ip_hdr;
	uint64_t ol_flags;
	uint16_t frag_field;

	for (i = 0; i != nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		if (!(ol_flags & PKT_TX_IPV4))
			continue;

		/* If there was not L2 header length specified, assume it is
		 * length of the ethernet header.
		 */
		if (unlikely(m->l2_len == 0))
			m->l2_len = sizeof(struct ether_hdr);

		ip_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
						 m->l2_len);
		frag_field = rte_be_to_cpu_16(ip_hdr->fragment_offset);

		if ((frag_field & IPV4_HDR_DF_FLAG) != 0) {
			m->packet_type |= RTE_PTYPE_L4_NONFRAG;

			/* If IPv4 header has DF flag enabled and TSO support is
			 * disabled, partial chcecksum should not be calculated.
			 */
			if (!tx_ring->adapter->tso4_supported)
				continue;
		}

		if ((ol_flags & ENA_TX_OFFLOAD_NOTSUP_MASK) != 0 ||
				(ol_flags & PKT_TX_L4_MASK) ==
				PKT_TX_SCTP_CKSUM) {
			rte_errno = -ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
#endif

		/* In case we are supposed to TSO and have DF not set (DF=0)
		 * hardware must be provided with partial checksum, otherwise
		 * it will take care of necessary calculations.
		 */

		ret = rte_net_intel_cksum_flags_prepare(m,
			ol_flags & ~PKT_TX_TCP_SEG);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
	}

	return i;
}

static uint16_t eth_ena_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts)
{
	struct ena_ring *tx_ring = (struct ena_ring *)(tx_queue);
	uint16_t next_to_use = tx_ring->next_to_use;
	uint16_t next_to_clean = tx_ring->next_to_clean;
	struct rte_mbuf *mbuf;
	unsigned int ring_size = tx_ring->ring_size;
	unsigned int ring_mask = ring_size - 1;
	struct ena_com_tx_ctx ena_tx_ctx;
	struct ena_tx_buffer *tx_info;
	struct ena_com_buf *ebuf;
	uint16_t rc, req_id, total_tx_descs = 0;
	uint16_t sent_idx = 0, empty_tx_reqs;
	int nb_hw_desc;

	/* Check adapter state */
	if (unlikely(tx_ring->adapter->state != ENA_ADAPTER_STATE_RUNNING)) {
		RTE_LOG(ALERT, PMD,
			"Trying to xmit pkts while device is NOT running\n");
		return 0;
	}

	empty_tx_reqs = ring_size - (next_to_use - next_to_clean);
	if (nb_pkts > empty_tx_reqs)
		nb_pkts = empty_tx_reqs;

	for (sent_idx = 0; sent_idx < nb_pkts; sent_idx++) {
		mbuf = tx_pkts[sent_idx];

		req_id = tx_ring->empty_tx_reqs[next_to_use & ring_mask];
		tx_info = &tx_ring->tx_buffer_info[req_id];
		tx_info->mbuf = mbuf;
		tx_info->num_of_bufs = 0;
		ebuf = tx_info->bufs;

		/* Prepare TX context */
		memset(&ena_tx_ctx, 0x0, sizeof(struct ena_com_tx_ctx));
		memset(&ena_tx_ctx.ena_meta, 0x0,
		       sizeof(struct ena_com_tx_meta));
		ena_tx_ctx.ena_bufs = ebuf;
		ena_tx_ctx.req_id = req_id;
		if (tx_ring->tx_mem_queue_type ==
				ENA_ADMIN_PLACEMENT_POLICY_DEV) {
			/* prepare the push buffer with
			 * virtual address of the data
			 */
			ena_tx_ctx.header_len =
				RTE_MIN(mbuf->data_len,
					tx_ring->tx_max_header_size);
			ena_tx_ctx.push_header =
				(void *)((char *)mbuf->buf_addr +
					 mbuf->data_off);
		} /* there's no else as we take advantage of memset zeroing */

		/* Set TX offloads flags, if applicable */
		ena_tx_mbuf_prepare(mbuf, &ena_tx_ctx);

		if (unlikely(mbuf->ol_flags &
			     (PKT_RX_L4_CKSUM_BAD | PKT_RX_IP_CKSUM_BAD)))
			rte_atomic64_inc(&tx_ring->adapter->drv_stats->ierrors);

		rte_prefetch0(tx_pkts[(sent_idx + 4) & ring_mask]);

		/* Process first segment taking into
		 * consideration pushed header
		 */
		if (mbuf->data_len > ena_tx_ctx.header_len) {
			ebuf->paddr = mbuf->buf_physaddr +
				      mbuf->data_off +
				      ena_tx_ctx.header_len;
			ebuf->len = mbuf->data_len - ena_tx_ctx.header_len;
			ebuf++;
			tx_info->num_of_bufs++;
		}

		while ((mbuf = mbuf->next) != NULL) {
			ebuf->paddr = mbuf->buf_physaddr + mbuf->data_off;
			ebuf->len = mbuf->data_len;
			ebuf++;
			tx_info->num_of_bufs++;
		}

		ena_tx_ctx.num_bufs = tx_info->num_of_bufs;

		/* Write data to device */
		rc = ena_com_prepare_tx(tx_ring->ena_com_io_sq,
					&ena_tx_ctx, &nb_hw_desc);
		if (unlikely(rc))
			break;

		tx_info->tx_descs = nb_hw_desc;

		next_to_use++;
	}

	/* If there are ready packets to be xmitted... */
	if (sent_idx > 0) {
		/* ...let HW do its best :-) */
		rte_wmb();
		ena_com_write_sq_doorbell(tx_ring->ena_com_io_sq);

		tx_ring->next_to_use = next_to_use;
	}

	/* Clear complete packets  */
	while (ena_com_tx_comp_req_id_get(tx_ring->ena_com_io_cq, &req_id) >= 0) {
		/* Get Tx info & store how many descs were processed  */
		tx_info = &tx_ring->tx_buffer_info[req_id];
		total_tx_descs += tx_info->tx_descs;

		/* Free whole mbuf chain  */
		mbuf = tx_info->mbuf;
		rte_pktmbuf_free(mbuf);

		/* Put back descriptor to the ring for reuse */
		tx_ring->empty_tx_reqs[next_to_clean & ring_mask] = req_id;
		next_to_clean++;

		/* If too many descs to clean, leave it for another run */
		if (unlikely(total_tx_descs > ENA_RING_DESCS_RATIO(ring_size)))
			break;
	}

	if (total_tx_descs > 0) {
		/* acknowledge completion of sent packets */
		ena_com_comp_ack(tx_ring->ena_com_io_sq, total_tx_descs);
		tx_ring->next_to_clean = next_to_clean;
	}

	return sent_idx;
}

static int eth_ena_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct ena_adapter), eth_ena_dev_init);
}

static int eth_ena_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, NULL);
}

static struct rte_pci_driver rte_ena_pmd = {
	.id_table = pci_id_ena_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_ena_pci_probe,
	.remove = eth_ena_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ena, rte_ena_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ena, pci_id_ena_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ena, "* igb_uio | uio_pci_generic | vfio-pci");
