/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#include <rte_string_fns.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_tcp.h>
#include <rte_atomic.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_version.h>
#include <rte_net.h>
#include <rte_kvargs.h>

#include "ena_ethdev.h"
#include "ena_logs.h"
#include "ena_platform.h"
#include "ena_com.h"
#include "ena_eth_com.h"

#include <ena_common_defs.h>
#include <ena_regs_defs.h>
#include <ena_admin_defs.h>
#include <ena_eth_io_defs.h>

#define DRV_MODULE_VER_MAJOR	2
#define DRV_MODULE_VER_MINOR	2
#define DRV_MODULE_VER_SUBMINOR	1

#define ENA_IO_TXQ_IDX(q)	(2 * (q))
#define ENA_IO_RXQ_IDX(q)	(2 * (q) + 1)
/*reverse version of ENA_IO_RXQ_IDX*/
#define ENA_IO_RXQ_IDX_REV(q)	((q - 1) / 2)

#define __MERGE_64B_H_L(h, l) (((uint64_t)h << 32) | l)
#define TEST_BIT(val, bit_shift) (val & (1UL << bit_shift))

#define GET_L4_HDR_LEN(mbuf)					\
	((rte_pktmbuf_mtod_offset(mbuf,	struct rte_tcp_hdr *,	\
		mbuf->l3_len + mbuf->l2_len)->data_off) >> 4)

#define ENA_RX_RSS_TABLE_LOG_SIZE  7
#define ENA_RX_RSS_TABLE_SIZE	(1 << ENA_RX_RSS_TABLE_LOG_SIZE)
#define ENA_HASH_KEY_SIZE	40
#define ETH_GSTRING_LEN	32

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ENA_MIN_RING_DESC	128

#define ENA_PTYPE_HAS_HASH	(RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP)

enum ethtool_stringset {
	ETH_SS_TEST             = 0,
	ETH_SS_STATS,
};

struct ena_stats {
	char name[ETH_GSTRING_LEN];
	int stat_offset;
};

#define ENA_STAT_ENTRY(stat, stat_type) { \
	.name = #stat, \
	.stat_offset = offsetof(struct ena_stats_##stat_type, stat) \
}

#define ENA_STAT_RX_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, rx)

#define ENA_STAT_TX_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, tx)

#define ENA_STAT_ENI_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, eni)

#define ENA_STAT_GLOBAL_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, dev)

/* Device arguments */
#define ENA_DEVARG_LARGE_LLQ_HDR "large_llq_hdr"

/*
 * Each rte_memzone should have unique name.
 * To satisfy it, count number of allocation and add it to name.
 */
rte_atomic32_t ena_alloc_cnt;

static const struct ena_stats ena_stats_global_strings[] = {
	ENA_STAT_GLOBAL_ENTRY(wd_expired),
	ENA_STAT_GLOBAL_ENTRY(dev_start),
	ENA_STAT_GLOBAL_ENTRY(dev_stop),
	ENA_STAT_GLOBAL_ENTRY(tx_drops),
};

static const struct ena_stats ena_stats_eni_strings[] = {
	ENA_STAT_ENI_ENTRY(bw_in_allowance_exceeded),
	ENA_STAT_ENI_ENTRY(bw_out_allowance_exceeded),
	ENA_STAT_ENI_ENTRY(pps_allowance_exceeded),
	ENA_STAT_ENI_ENTRY(conntrack_allowance_exceeded),
	ENA_STAT_ENI_ENTRY(linklocal_allowance_exceeded),
};

static const struct ena_stats ena_stats_tx_strings[] = {
	ENA_STAT_TX_ENTRY(cnt),
	ENA_STAT_TX_ENTRY(bytes),
	ENA_STAT_TX_ENTRY(prepare_ctx_err),
	ENA_STAT_TX_ENTRY(linearize),
	ENA_STAT_TX_ENTRY(linearize_failed),
	ENA_STAT_TX_ENTRY(tx_poll),
	ENA_STAT_TX_ENTRY(doorbells),
	ENA_STAT_TX_ENTRY(bad_req_id),
	ENA_STAT_TX_ENTRY(available_desc),
};

static const struct ena_stats ena_stats_rx_strings[] = {
	ENA_STAT_RX_ENTRY(cnt),
	ENA_STAT_RX_ENTRY(bytes),
	ENA_STAT_RX_ENTRY(refill_partial),
	ENA_STAT_RX_ENTRY(bad_csum),
	ENA_STAT_RX_ENTRY(mbuf_alloc_fail),
	ENA_STAT_RX_ENTRY(bad_desc_num),
	ENA_STAT_RX_ENTRY(bad_req_id),
};

#define ENA_STATS_ARRAY_GLOBAL	ARRAY_SIZE(ena_stats_global_strings)
#define ENA_STATS_ARRAY_ENI	ARRAY_SIZE(ena_stats_eni_strings)
#define ENA_STATS_ARRAY_TX	ARRAY_SIZE(ena_stats_tx_strings)
#define ENA_STATS_ARRAY_RX	ARRAY_SIZE(ena_stats_rx_strings)

#define QUEUE_OFFLOADS (DEV_TX_OFFLOAD_TCP_CKSUM |\
			DEV_TX_OFFLOAD_UDP_CKSUM |\
			DEV_TX_OFFLOAD_IPV4_CKSUM |\
			DEV_TX_OFFLOAD_TCP_TSO)
#define MBUF_OFFLOADS (PKT_TX_L4_MASK |\
		       PKT_TX_IP_CKSUM |\
		       PKT_TX_TCP_SEG)

/** Vendor ID used by Amazon devices */
#define PCI_VENDOR_ID_AMAZON 0x1D0F
/** Amazon devices */
#define PCI_DEVICE_ID_ENA_VF		0xEC20
#define PCI_DEVICE_ID_ENA_VF_RSERV0	0xEC21

#define	ENA_TX_OFFLOAD_MASK	(\
	PKT_TX_L4_MASK |         \
	PKT_TX_IPV6 |            \
	PKT_TX_IPV4 |            \
	PKT_TX_IP_CKSUM |        \
	PKT_TX_TCP_SEG)

#define	ENA_TX_OFFLOAD_NOTSUP_MASK	\
	(PKT_TX_OFFLOAD_MASK ^ ENA_TX_OFFLOAD_MASK)

/** HW specific offloads capabilities. */
/* IPv4 checksum offload. */
#define ENA_L3_IPV4_CSUM		0x0001
/* TCP/UDP checksum offload for IPv4 packets. */
#define ENA_L4_IPV4_CSUM		0x0002
/* TCP/UDP checksum offload for IPv4 packets with pseudo header checksum. */
#define ENA_L4_IPV4_CSUM_PARTIAL	0x0004
/* TCP/UDP checksum offload for IPv6 packets. */
#define ENA_L4_IPV6_CSUM		0x0008
/* TCP/UDP checksum offload for IPv6 packets with pseudo header checksum. */
#define ENA_L4_IPV6_CSUM_PARTIAL	0x0010
/* TSO support for IPv4 packets. */
#define ENA_IPV4_TSO			0x0020

/* Device supports setting RSS hash. */
#define ENA_RX_RSS_HASH			0x0040

static const struct rte_pci_id pci_id_ena_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AMAZON, PCI_DEVICE_ID_ENA_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_AMAZON, PCI_DEVICE_ID_ENA_VF_RSERV0) },
	{ .device_id = 0 },
};

static struct ena_aenq_handlers aenq_handlers;

static int ena_device_init(struct ena_com_dev *ena_dev,
			   struct ena_com_dev_get_features_ctx *get_feat_ctx,
			   bool *wd_state);
static int ena_dev_configure(struct rte_eth_dev *dev);
static void ena_tx_map_mbuf(struct ena_ring *tx_ring,
	struct ena_tx_buffer *tx_info,
	struct rte_mbuf *mbuf,
	void **push_header,
	uint16_t *header_len);
static int ena_xmit_mbuf(struct ena_ring *tx_ring, struct rte_mbuf *mbuf);
static void ena_tx_cleanup(struct ena_ring *tx_ring);
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
static inline void ena_init_rx_mbuf(struct rte_mbuf *mbuf, uint16_t len);
static struct rte_mbuf *ena_rx_mbuf(struct ena_ring *rx_ring,
				    struct ena_com_rx_buf_info *ena_bufs,
				    uint32_t descs,
				    uint16_t *next_to_clean,
				    uint8_t offset);
static uint16_t eth_ena_recv_pkts(void *rx_queue,
				  struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
static int ena_add_single_rx_desc(struct ena_com_io_sq *io_sq,
				  struct rte_mbuf *mbuf, uint16_t id);
static int ena_populate_rx_queue(struct ena_ring *rxq, unsigned int count);
static void ena_init_rings(struct ena_adapter *adapter,
			   bool disable_meta_caching);
static int ena_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int ena_start(struct rte_eth_dev *dev);
static int ena_stop(struct rte_eth_dev *dev);
static int ena_close(struct rte_eth_dev *dev);
static int ena_dev_reset(struct rte_eth_dev *dev);
static int ena_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
static void ena_rx_queue_release_all(struct rte_eth_dev *dev);
static void ena_tx_queue_release_all(struct rte_eth_dev *dev);
static void ena_rx_queue_release(void *queue);
static void ena_tx_queue_release(void *queue);
static void ena_rx_queue_release_bufs(struct ena_ring *ring);
static void ena_tx_queue_release_bufs(struct ena_ring *ring);
static int ena_link_update(struct rte_eth_dev *dev,
			   int wait_to_complete);
static int ena_create_io_queue(struct ena_ring *ring);
static void ena_queue_stop(struct ena_ring *ring);
static void ena_queue_stop_all(struct rte_eth_dev *dev,
			      enum ena_ring_type ring_type);
static int ena_queue_start(struct ena_ring *ring);
static int ena_queue_start_all(struct rte_eth_dev *dev,
			       enum ena_ring_type ring_type);
static void ena_stats_restart(struct rte_eth_dev *dev);
static uint64_t ena_get_rx_port_offloads(struct ena_adapter *adapter);
static uint64_t ena_get_tx_port_offloads(struct ena_adapter *adapter);
static uint64_t ena_get_rx_queue_offloads(struct ena_adapter *adapter);
static uint64_t ena_get_tx_queue_offloads(struct ena_adapter *adapter);
static int ena_infos_get(struct rte_eth_dev *dev,
			 struct rte_eth_dev_info *dev_info);
static int ena_rss_reta_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);
static int ena_rss_reta_query(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size);
static void ena_interrupt_handler_rte(void *cb_arg);
static void ena_timer_wd_callback(struct rte_timer *timer, void *arg);
static void ena_destroy_device(struct rte_eth_dev *eth_dev);
static int eth_ena_dev_init(struct rte_eth_dev *eth_dev);
static int ena_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int n);
static int ena_xstats_get(struct rte_eth_dev *dev,
			  struct rte_eth_xstat *stats,
			  unsigned int n);
static int ena_xstats_get_by_id(struct rte_eth_dev *dev,
				const uint64_t *ids,
				uint64_t *values,
				unsigned int n);
static int ena_process_bool_devarg(const char *key,
				   const char *value,
				   void *opaque);
static int ena_parse_devargs(struct ena_adapter *adapter,
			     struct rte_devargs *devargs);
static int ena_copy_eni_stats(struct ena_adapter *adapter);

static const struct eth_dev_ops ena_dev_ops = {
	.dev_configure        = ena_dev_configure,
	.dev_infos_get        = ena_infos_get,
	.rx_queue_setup       = ena_rx_queue_setup,
	.tx_queue_setup       = ena_tx_queue_setup,
	.dev_start            = ena_start,
	.dev_stop             = ena_stop,
	.link_update          = ena_link_update,
	.stats_get            = ena_stats_get,
	.xstats_get_names     = ena_xstats_get_names,
	.xstats_get	      = ena_xstats_get,
	.xstats_get_by_id     = ena_xstats_get_by_id,
	.mtu_set              = ena_mtu_set,
	.rx_queue_release     = ena_rx_queue_release,
	.tx_queue_release     = ena_tx_queue_release,
	.dev_close            = ena_close,
	.dev_reset            = ena_dev_reset,
	.reta_update          = ena_rss_reta_update,
	.reta_query           = ena_rss_reta_query,
};

void ena_rss_key_fill(void *key, size_t size)
{
	static bool key_generated;
	static uint8_t default_key[ENA_HASH_KEY_SIZE];
	size_t i;

	RTE_ASSERT(size <= ENA_HASH_KEY_SIZE);

	if (!key_generated) {
		for (i = 0; i < ENA_HASH_KEY_SIZE; ++i)
			default_key[i] = rte_rand() & 0xff;
		key_generated = true;
	}

	rte_memcpy(key, default_key, size);
}

static inline void ena_rx_mbuf_prepare(struct rte_mbuf *mbuf,
				       struct ena_com_rx_ctx *ena_rx_ctx)
{
	uint64_t ol_flags = 0;
	uint32_t packet_type = 0;

	if (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_TCP)
		packet_type |= RTE_PTYPE_L4_TCP;
	else if (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_UDP)
		packet_type |= RTE_PTYPE_L4_UDP;

	if (ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV4) {
		packet_type |= RTE_PTYPE_L3_IPV4;
		if (unlikely(ena_rx_ctx->l3_csum_err))
			ol_flags |= PKT_RX_IP_CKSUM_BAD;
		else
			ol_flags |= PKT_RX_IP_CKSUM_GOOD;
	} else if (ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV6) {
		packet_type |= RTE_PTYPE_L3_IPV6;
	}

	if (!ena_rx_ctx->l4_csum_checked || ena_rx_ctx->frag)
		ol_flags |= PKT_RX_L4_CKSUM_UNKNOWN;
	else
		if (unlikely(ena_rx_ctx->l4_csum_err))
			ol_flags |= PKT_RX_L4_CKSUM_BAD;
		else
			ol_flags |= PKT_RX_L4_CKSUM_GOOD;

	if (likely((packet_type & ENA_PTYPE_HAS_HASH) && !ena_rx_ctx->frag)) {
		ol_flags |= PKT_RX_RSS_HASH;
		mbuf->hash.rss = ena_rx_ctx->hash;
	}

	mbuf->ol_flags = ol_flags;
	mbuf->packet_type = packet_type;
}

static inline void ena_tx_mbuf_prepare(struct rte_mbuf *mbuf,
				       struct ena_com_tx_ctx *ena_tx_ctx,
				       uint64_t queue_offloads,
				       bool disable_meta_caching)
{
	struct ena_com_tx_meta *ena_meta = &ena_tx_ctx->ena_meta;

	if ((mbuf->ol_flags & MBUF_OFFLOADS) &&
	    (queue_offloads & QUEUE_OFFLOADS)) {
		/* check if TSO is required */
		if ((mbuf->ol_flags & PKT_TX_TCP_SEG) &&
		    (queue_offloads & DEV_TX_OFFLOAD_TCP_TSO)) {
			ena_tx_ctx->tso_enable = true;

			ena_meta->l4_hdr_len = GET_L4_HDR_LEN(mbuf);
		}

		/* check if L3 checksum is needed */
		if ((mbuf->ol_flags & PKT_TX_IP_CKSUM) &&
		    (queue_offloads & DEV_TX_OFFLOAD_IPV4_CKSUM))
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
		if (((mbuf->ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM) &&
		    (queue_offloads & DEV_TX_OFFLOAD_TCP_CKSUM)) {
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_TCP;
			ena_tx_ctx->l4_csum_enable = true;
		} else if (((mbuf->ol_flags & PKT_TX_L4_MASK) ==
				PKT_TX_UDP_CKSUM) &&
				(queue_offloads & DEV_TX_OFFLOAD_UDP_CKSUM)) {
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UDP;
			ena_tx_ctx->l4_csum_enable = true;
		} else {
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UNKNOWN;
			ena_tx_ctx->l4_csum_enable = false;
		}

		ena_meta->mss = mbuf->tso_segsz;
		ena_meta->l3_hdr_len = mbuf->l3_len;
		ena_meta->l3_hdr_offset = mbuf->l2_len;

		ena_tx_ctx->meta_valid = true;
	} else if (disable_meta_caching) {
		memset(ena_meta, 0, sizeof(*ena_meta));
		ena_tx_ctx->meta_valid = true;
	} else {
		ena_tx_ctx->meta_valid = false;
	}
}

static int validate_tx_req_id(struct ena_ring *tx_ring, u16 req_id)
{
	struct ena_tx_buffer *tx_info = NULL;

	if (likely(req_id < tx_ring->ring_size)) {
		tx_info = &tx_ring->tx_buffer_info[req_id];
		if (likely(tx_info->mbuf))
			return 0;
	}

	if (tx_info)
		PMD_DRV_LOG(ERR, "tx_info doesn't have valid mbuf\n");
	else
		PMD_DRV_LOG(ERR, "Invalid req_id: %hu\n", req_id);

	/* Trigger device reset */
	++tx_ring->tx_stats.bad_req_id;
	tx_ring->adapter->reset_reason = ENA_REGS_RESET_INV_TX_REQ_ID;
	tx_ring->adapter->trigger_reset	= true;
	return -EFAULT;
}

static void ena_config_host_info(struct ena_com_dev *ena_dev)
{
	struct ena_admin_host_info *host_info;
	int rc;

	/* Allocate only the host info */
	rc = ena_com_allocate_host_info(ena_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Cannot allocate host info\n");
		return;
	}

	host_info = ena_dev->host_attr.host_info;

	host_info->os_type = ENA_ADMIN_OS_DPDK;
	host_info->kernel_ver = RTE_VERSION;
	strlcpy((char *)host_info->kernel_ver_str, rte_version(),
		sizeof(host_info->kernel_ver_str));
	host_info->os_dist = RTE_VERSION;
	strlcpy((char *)host_info->os_dist_str, rte_version(),
		sizeof(host_info->os_dist_str));
	host_info->driver_version =
		(DRV_MODULE_VER_MAJOR) |
		(DRV_MODULE_VER_MINOR << ENA_ADMIN_HOST_INFO_MINOR_SHIFT) |
		(DRV_MODULE_VER_SUBMINOR <<
			ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT);
	host_info->num_cpus = rte_lcore_count();

	host_info->driver_supported_features =
		ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK;

	rc = ena_com_set_host_attributes(ena_dev);
	if (rc) {
		if (rc == -ENA_COM_UNSUPPORTED)
			PMD_DRV_LOG(WARNING, "Cannot set host attributes\n");
		else
			PMD_DRV_LOG(ERR, "Cannot set host attributes\n");

		goto err;
	}

	return;

err:
	ena_com_delete_host_info(ena_dev);
}

/* This function calculates the number of xstats based on the current config */
static unsigned int ena_xstats_calc_num(struct rte_eth_dev *dev)
{
	return ENA_STATS_ARRAY_GLOBAL + ENA_STATS_ARRAY_ENI +
		(dev->data->nb_tx_queues * ENA_STATS_ARRAY_TX) +
		(dev->data->nb_rx_queues * ENA_STATS_ARRAY_RX);
}

static void ena_config_debug_area(struct ena_adapter *adapter)
{
	u32 debug_area_size;
	int rc, ss_count;

	ss_count = ena_xstats_calc_num(adapter->rte_dev);

	/* allocate 32 bytes for each string and 64bit for the value */
	debug_area_size = ss_count * ETH_GSTRING_LEN + sizeof(u64) * ss_count;

	rc = ena_com_allocate_debug_area(&adapter->ena_dev, debug_area_size);
	if (rc) {
		PMD_DRV_LOG(ERR, "Cannot allocate debug area\n");
		return;
	}

	rc = ena_com_set_host_attributes(&adapter->ena_dev);
	if (rc) {
		if (rc == -ENA_COM_UNSUPPORTED)
			PMD_DRV_LOG(WARNING, "Cannot set host attributes\n");
		else
			PMD_DRV_LOG(ERR, "Cannot set host attributes\n");

		goto err;
	}

	return;
err:
	ena_com_delete_debug_area(&adapter->ena_dev);
}

static int ena_close(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ena_adapter *adapter = dev->data->dev_private;
	int ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (adapter->state == ENA_ADAPTER_STATE_RUNNING)
		ret = ena_stop(dev);
	adapter->state = ENA_ADAPTER_STATE_CLOSED;

	ena_rx_queue_release_all(dev);
	ena_tx_queue_release_all(dev);

	rte_free(adapter->drv_stats);
	adapter->drv_stats = NULL;

	rte_intr_disable(intr_handle);
	rte_intr_callback_unregister(intr_handle,
				     ena_interrupt_handler_rte,
				     adapter);

	/*
	 * MAC is not allocated dynamically. Setting NULL should prevent from
	 * release of the resource in the rte_eth_dev_release_port().
	 */
	dev->data->mac_addrs = NULL;

	return ret;
}

static int
ena_dev_reset(struct rte_eth_dev *dev)
{
	int rc = 0;

	ena_destroy_device(dev);
	rc = eth_ena_dev_init(dev);
	if (rc)
		PMD_INIT_LOG(CRIT, "Cannot initialize device");

	return rc;
}

static int ena_rss_reta_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc, i;
	u16 entry_value;
	int conf_idx;
	int idx;

	if ((reta_size == 0) || (reta_conf == NULL))
		return -EINVAL;

	if (reta_size > ENA_RX_RSS_TABLE_SIZE) {
		PMD_DRV_LOG(WARNING,
			"indirection table %d is bigger than supported (%d)\n",
			reta_size, ENA_RX_RSS_TABLE_SIZE);
		return -EINVAL;
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

			rc = ena_com_indirect_table_fill_entry(ena_dev,
							       i,
							       entry_value);
			if (unlikely(rc && rc != ENA_COM_UNSUPPORTED)) {
				PMD_DRV_LOG(ERR,
					"Cannot fill indirect table\n");
				return rc;
			}
		}
	}

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ena_com_indirect_table_set(ena_dev);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (unlikely(rc && rc != ENA_COM_UNSUPPORTED)) {
		PMD_DRV_LOG(ERR, "Cannot flush the indirect table\n");
		return rc;
	}

	PMD_DRV_LOG(DEBUG, "%s(): RSS configured %d entries  for port %d\n",
		__func__, reta_size, adapter->rte_dev->data->port_id);

	return 0;
}

/* Query redirection table. */
static int ena_rss_reta_query(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc;
	int i;
	u32 indirect_table[ENA_RX_RSS_TABLE_SIZE] = {0};
	int reta_conf_idx;
	int reta_idx;

	if (reta_size == 0 || reta_conf == NULL ||
	    (reta_size > RTE_RETA_GROUP_SIZE && ((reta_conf + 1) == NULL)))
		return -EINVAL;

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ena_com_indirect_table_get(ena_dev, indirect_table);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (unlikely(rc && rc != ENA_COM_UNSUPPORTED)) {
		PMD_DRV_LOG(ERR, "cannot get indirect table\n");
		return -ENOTSUP;
	}

	for (i = 0 ; i < reta_size ; i++) {
		reta_conf_idx = i / RTE_RETA_GROUP_SIZE;
		reta_idx = i % RTE_RETA_GROUP_SIZE;
		if (TEST_BIT(reta_conf[reta_conf_idx].mask, reta_idx))
			reta_conf[reta_conf_idx].reta[reta_idx] =
				ENA_IO_RXQ_IDX_REV(indirect_table[i]);
	}

	return 0;
}

static int ena_rss_init_default(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	uint16_t nb_rx_queues = adapter->rte_dev->data->nb_rx_queues;
	int rc, i;
	u32 val;

	rc = ena_com_rss_init(ena_dev, ENA_RX_RSS_TABLE_LOG_SIZE);
	if (unlikely(rc)) {
		PMD_DRV_LOG(ERR, "Cannot init indirect table\n");
		goto err_rss_init;
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; i++) {
		val = i % nb_rx_queues;
		rc = ena_com_indirect_table_fill_entry(ena_dev, i,
						       ENA_IO_RXQ_IDX(val));
		if (unlikely(rc && (rc != ENA_COM_UNSUPPORTED))) {
			PMD_DRV_LOG(ERR, "Cannot fill indirect table\n");
			goto err_fill_indir;
		}
	}

	rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_CRC32, NULL,
					ENA_HASH_KEY_SIZE, 0xFFFFFFFF);
	if (unlikely(rc && (rc != ENA_COM_UNSUPPORTED))) {
		PMD_DRV_LOG(INFO, "Cannot fill hash function\n");
		goto err_fill_indir;
	}

	rc = ena_com_set_default_hash_ctrl(ena_dev);
	if (unlikely(rc && (rc != ENA_COM_UNSUPPORTED))) {
		PMD_DRV_LOG(INFO, "Cannot fill hash control\n");
		goto err_fill_indir;
	}

	rc = ena_com_indirect_table_set(ena_dev);
	if (unlikely(rc && (rc != ENA_COM_UNSUPPORTED))) {
		PMD_DRV_LOG(ERR, "Cannot flush the indirect table\n");
		goto err_fill_indir;
	}
	PMD_DRV_LOG(DEBUG, "RSS configured for port %d\n",
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

	/* Free ring resources */
	if (ring->rx_buffer_info)
		rte_free(ring->rx_buffer_info);
	ring->rx_buffer_info = NULL;

	if (ring->rx_refill_buffer)
		rte_free(ring->rx_refill_buffer);
	ring->rx_refill_buffer = NULL;

	if (ring->empty_rx_reqs)
		rte_free(ring->empty_rx_reqs);
	ring->empty_rx_reqs = NULL;

	ring->configured = 0;

	PMD_DRV_LOG(NOTICE, "RX Queue %d:%d released\n",
		ring->port_id, ring->id);
}

static void ena_tx_queue_release(void *queue)
{
	struct ena_ring *ring = (struct ena_ring *)queue;

	/* Free ring resources */
	if (ring->push_buf_intermediate_buf)
		rte_free(ring->push_buf_intermediate_buf);

	if (ring->tx_buffer_info)
		rte_free(ring->tx_buffer_info);

	if (ring->empty_tx_reqs)
		rte_free(ring->empty_tx_reqs);

	ring->empty_tx_reqs = NULL;
	ring->tx_buffer_info = NULL;
	ring->push_buf_intermediate_buf = NULL;

	ring->configured = 0;

	PMD_DRV_LOG(NOTICE, "TX Queue %d:%d released\n",
		ring->port_id, ring->id);
}

static void ena_rx_queue_release_bufs(struct ena_ring *ring)
{
	unsigned int i;

	for (i = 0; i < ring->ring_size; ++i) {
		struct ena_rx_buffer *rx_info = &ring->rx_buffer_info[i];
		if (rx_info->mbuf) {
			rte_mbuf_raw_free(rx_info->mbuf);
			rx_info->mbuf = NULL;
		}
	}
}

static void ena_tx_queue_release_bufs(struct ena_ring *ring)
{
	unsigned int i;

	for (i = 0; i < ring->ring_size; ++i) {
		struct ena_tx_buffer *tx_buf = &ring->tx_buffer_info[i];

		if (tx_buf->mbuf) {
			rte_pktmbuf_free(tx_buf->mbuf);
			tx_buf->mbuf = NULL;
		}
	}
}

static int ena_link_update(struct rte_eth_dev *dev,
			   __rte_unused int wait_to_complete)
{
	struct rte_eth_link *link = &dev->data->dev_link;
	struct ena_adapter *adapter = dev->data->dev_private;

	link->link_status = adapter->link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
	link->link_speed = ETH_SPEED_NUM_NONE;
	link->link_duplex = ETH_LINK_FULL_DUPLEX;

	return 0;
}

static int ena_queue_start_all(struct rte_eth_dev *dev,
			       enum ena_ring_type ring_type)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_ring *queues = NULL;
	int nb_queues;
	int i = 0;
	int rc = 0;

	if (ring_type == ENA_RING_TYPE_RX) {
		queues = adapter->rx_ring;
		nb_queues = dev->data->nb_rx_queues;
	} else {
		queues = adapter->tx_ring;
		nb_queues = dev->data->nb_tx_queues;
	}
	for (i = 0; i < nb_queues; i++) {
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

			rc = ena_queue_start(&queues[i]);

			if (rc) {
				PMD_INIT_LOG(ERR,
					     "failed to start queue %d type(%d)",
					     i, ring_type);
				goto err;
			}
		}
	}

	return 0;

err:
	while (i--)
		if (queues[i].configured)
			ena_queue_stop(&queues[i]);

	return rc;
}

static uint32_t ena_get_mtu_conf(struct ena_adapter *adapter)
{
	uint32_t max_frame_len = adapter->max_mtu;

	if (adapter->rte_eth_dev_data->dev_conf.rxmode.offloads &
	    DEV_RX_OFFLOAD_JUMBO_FRAME)
		max_frame_len =
			adapter->rte_eth_dev_data->dev_conf.rxmode.max_rx_pkt_len;

	return max_frame_len;
}

static int ena_check_valid_conf(struct ena_adapter *adapter)
{
	uint32_t max_frame_len = ena_get_mtu_conf(adapter);

	if (max_frame_len > adapter->max_mtu || max_frame_len < ENA_MIN_MTU) {
		PMD_INIT_LOG(ERR, "Unsupported MTU of %d. "
				  "max mtu: %d, min mtu: %d",
			     max_frame_len, adapter->max_mtu, ENA_MIN_MTU);
		return ENA_COM_UNSUPPORTED;
	}

	return 0;
}

static int
ena_calc_io_queue_size(struct ena_calc_queue_size_ctx *ctx,
		       bool use_large_llq_hdr)
{
	struct ena_admin_feature_llq_desc *llq = &ctx->get_feat_ctx->llq;
	struct ena_com_dev *ena_dev = ctx->ena_dev;
	uint32_t max_tx_queue_size;
	uint32_t max_rx_queue_size;

	if (ena_dev->supported_features & BIT(ENA_ADMIN_MAX_QUEUES_EXT)) {
		struct ena_admin_queue_ext_feature_fields *max_queue_ext =
			&ctx->get_feat_ctx->max_queue_ext.max_queue_ext;
		max_rx_queue_size = RTE_MIN(max_queue_ext->max_rx_cq_depth,
			max_queue_ext->max_rx_sq_depth);
		max_tx_queue_size = max_queue_ext->max_tx_cq_depth;

		if (ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV) {
			max_tx_queue_size = RTE_MIN(max_tx_queue_size,
				llq->max_llq_depth);
		} else {
			max_tx_queue_size = RTE_MIN(max_tx_queue_size,
				max_queue_ext->max_tx_sq_depth);
		}

		ctx->max_rx_sgl_size = RTE_MIN(ENA_PKT_MAX_BUFS,
			max_queue_ext->max_per_packet_rx_descs);
		ctx->max_tx_sgl_size = RTE_MIN(ENA_PKT_MAX_BUFS,
			max_queue_ext->max_per_packet_tx_descs);
	} else {
		struct ena_admin_queue_feature_desc *max_queues =
			&ctx->get_feat_ctx->max_queues;
		max_rx_queue_size = RTE_MIN(max_queues->max_cq_depth,
			max_queues->max_sq_depth);
		max_tx_queue_size = max_queues->max_cq_depth;

		if (ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV) {
			max_tx_queue_size = RTE_MIN(max_tx_queue_size,
				llq->max_llq_depth);
		} else {
			max_tx_queue_size = RTE_MIN(max_tx_queue_size,
				max_queues->max_sq_depth);
		}

		ctx->max_rx_sgl_size = RTE_MIN(ENA_PKT_MAX_BUFS,
			max_queues->max_packet_rx_descs);
		ctx->max_tx_sgl_size = RTE_MIN(ENA_PKT_MAX_BUFS,
			max_queues->max_packet_tx_descs);
	}

	/* Round down to the nearest power of 2 */
	max_rx_queue_size = rte_align32prevpow2(max_rx_queue_size);
	max_tx_queue_size = rte_align32prevpow2(max_tx_queue_size);

	if (use_large_llq_hdr) {
		if ((llq->entry_size_ctrl_supported &
		     ENA_ADMIN_LIST_ENTRY_SIZE_256B) &&
		    (ena_dev->tx_mem_queue_type ==
		     ENA_ADMIN_PLACEMENT_POLICY_DEV)) {
			max_tx_queue_size /= 2;
			PMD_INIT_LOG(INFO,
				"Forcing large headers and decreasing maximum TX queue size to %d\n",
				max_tx_queue_size);
		} else {
			PMD_INIT_LOG(ERR,
				"Forcing large headers failed: LLQ is disabled or device does not support large headers\n");
		}
	}

	if (unlikely(max_rx_queue_size == 0 || max_tx_queue_size == 0)) {
		PMD_INIT_LOG(ERR, "Invalid queue size");
		return -EFAULT;
	}

	ctx->max_tx_queue_size = max_tx_queue_size;
	ctx->max_rx_queue_size = max_rx_queue_size;

	return 0;
}

static void ena_stats_restart(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter = dev->data->dev_private;

	rte_atomic64_init(&adapter->drv_stats->ierrors);
	rte_atomic64_init(&adapter->drv_stats->oerrors);
	rte_atomic64_init(&adapter->drv_stats->rx_nombuf);
	adapter->drv_stats->rx_drops = 0;
}

static int ena_stats_get(struct rte_eth_dev *dev,
			  struct rte_eth_stats *stats)
{
	struct ena_admin_basic_stats ena_stats;
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc;
	int i;
	int max_rings_stats;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -ENOTSUP;

	memset(&ena_stats, 0, sizeof(ena_stats));

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ena_com_get_dev_basic_stats(ena_dev, &ena_stats);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (unlikely(rc)) {
		PMD_DRV_LOG(ERR, "Could not retrieve statistics from ENA\n");
		return rc;
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

	/* Driver related stats */
	stats->imissed = adapter->drv_stats->rx_drops;
	stats->ierrors = rte_atomic64_read(&adapter->drv_stats->ierrors);
	stats->oerrors = rte_atomic64_read(&adapter->drv_stats->oerrors);
	stats->rx_nombuf = rte_atomic64_read(&adapter->drv_stats->rx_nombuf);

	max_rings_stats = RTE_MIN(dev->data->nb_rx_queues,
		RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (i = 0; i < max_rings_stats; ++i) {
		struct ena_stats_rx *rx_stats = &adapter->rx_ring[i].rx_stats;

		stats->q_ibytes[i] = rx_stats->bytes;
		stats->q_ipackets[i] = rx_stats->cnt;
		stats->q_errors[i] = rx_stats->bad_desc_num +
			rx_stats->bad_req_id;
	}

	max_rings_stats = RTE_MIN(dev->data->nb_tx_queues,
		RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (i = 0; i < max_rings_stats; ++i) {
		struct ena_stats_tx *tx_stats = &adapter->tx_ring[i].tx_stats;

		stats->q_obytes[i] = tx_stats->bytes;
		stats->q_opackets[i] = tx_stats->cnt;
	}

	return 0;
}

static int ena_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev;
	int rc = 0;

	ena_assert_msg(dev->data != NULL, "Uninitialized device\n");
	ena_assert_msg(dev->data->dev_private != NULL, "Uninitialized device\n");
	adapter = dev->data->dev_private;

	ena_dev = &adapter->ena_dev;
	ena_assert_msg(ena_dev != NULL, "Uninitialized device\n");

	if (mtu > ena_get_mtu_conf(adapter) || mtu < ENA_MIN_MTU) {
		PMD_DRV_LOG(ERR,
			"Invalid MTU setting. new_mtu: %d "
			"max mtu: %d min mtu: %d\n",
			mtu, ena_get_mtu_conf(adapter), ENA_MIN_MTU);
		return -EINVAL;
	}

	rc = ena_com_set_dev_mtu(ena_dev, mtu);
	if (rc)
		PMD_DRV_LOG(ERR, "Could not set MTU: %d\n", mtu);
	else
		PMD_DRV_LOG(NOTICE, "Set MTU: %d\n", mtu);

	return rc;
}

static int ena_start(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	uint64_t ticks;
	int rc = 0;

	rc = ena_check_valid_conf(adapter);
	if (rc)
		return rc;

	rc = ena_queue_start_all(dev, ENA_RING_TYPE_RX);
	if (rc)
		return rc;

	rc = ena_queue_start_all(dev, ENA_RING_TYPE_TX);
	if (rc)
		goto err_start_tx;

	if (adapter->rte_dev->data->dev_conf.rxmode.mq_mode &
	    ETH_MQ_RX_RSS_FLAG && adapter->rte_dev->data->nb_rx_queues > 0) {
		rc = ena_rss_init_default(adapter);
		if (rc)
			goto err_rss_init;
	}

	ena_stats_restart(dev);

	adapter->timestamp_wd = rte_get_timer_cycles();
	adapter->keep_alive_timeout = ENA_DEVICE_KALIVE_TIMEOUT;

	ticks = rte_get_timer_hz();
	rte_timer_reset(&adapter->timer_wd, ticks, PERIODICAL, rte_lcore_id(),
			ena_timer_wd_callback, adapter);

	++adapter->dev_stats.dev_start;
	adapter->state = ENA_ADAPTER_STATE_RUNNING;

	return 0;

err_rss_init:
	ena_queue_stop_all(dev, ENA_RING_TYPE_TX);
err_start_tx:
	ena_queue_stop_all(dev, ENA_RING_TYPE_RX);
	return rc;
}

static int ena_stop(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc;

	rte_timer_stop_sync(&adapter->timer_wd);
	ena_queue_stop_all(dev, ENA_RING_TYPE_TX);
	ena_queue_stop_all(dev, ENA_RING_TYPE_RX);

	if (adapter->trigger_reset) {
		rc = ena_com_dev_reset(ena_dev, adapter->reset_reason);
		if (rc)
			PMD_DRV_LOG(ERR, "Device reset failed rc=%d\n", rc);
	}

	++adapter->dev_stats.dev_stop;
	adapter->state = ENA_ADAPTER_STATE_STOPPED;
	dev->data->dev_started = 0;

	return 0;
}

static int ena_create_io_queue(struct ena_ring *ring)
{
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev;
	struct ena_com_create_io_ctx ctx =
		/* policy set to _HOST just to satisfy icc compiler */
		{ ENA_ADMIN_PLACEMENT_POLICY_HOST,
		  0, 0, 0, 0, 0 };
	uint16_t ena_qid;
	unsigned int i;
	int rc;

	adapter = ring->adapter;
	ena_dev = &adapter->ena_dev;

	if (ring->type == ENA_RING_TYPE_TX) {
		ena_qid = ENA_IO_TXQ_IDX(ring->id);
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_TX;
		ctx.mem_queue_type = ena_dev->tx_mem_queue_type;
		for (i = 0; i < ring->ring_size; i++)
			ring->empty_tx_reqs[i] = i;
	} else {
		ena_qid = ENA_IO_RXQ_IDX(ring->id);
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_RX;
		for (i = 0; i < ring->ring_size; i++)
			ring->empty_rx_reqs[i] = i;
	}
	ctx.queue_size = ring->ring_size;
	ctx.qid = ena_qid;
	ctx.msix_vector = -1; /* interrupts not used */
	ctx.numa_node = ring->numa_socket_id;

	rc = ena_com_create_io_queue(ena_dev, &ctx);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"failed to create io queue #%d (qid:%d) rc: %d\n",
			ring->id, ena_qid, rc);
		return rc;
	}

	rc = ena_com_get_io_handlers(ena_dev, ena_qid,
				     &ring->ena_com_io_sq,
				     &ring->ena_com_io_cq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Failed to get io queue handlers. queue num %d rc: %d\n",
			ring->id, rc);
		ena_com_destroy_io_queue(ena_dev, ena_qid);
		return rc;
	}

	if (ring->type == ENA_RING_TYPE_TX)
		ena_com_update_numa_node(ring->ena_com_io_cq, ctx.numa_node);

	return 0;
}

static void ena_queue_stop(struct ena_ring *ring)
{
	struct ena_com_dev *ena_dev = &ring->adapter->ena_dev;

	if (ring->type == ENA_RING_TYPE_RX) {
		ena_com_destroy_io_queue(ena_dev, ENA_IO_RXQ_IDX(ring->id));
		ena_rx_queue_release_bufs(ring);
	} else {
		ena_com_destroy_io_queue(ena_dev, ENA_IO_TXQ_IDX(ring->id));
		ena_tx_queue_release_bufs(ring);
	}
}

static void ena_queue_stop_all(struct rte_eth_dev *dev,
			      enum ena_ring_type ring_type)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_ring *queues = NULL;
	uint16_t nb_queues, i;

	if (ring_type == ENA_RING_TYPE_RX) {
		queues = adapter->rx_ring;
		nb_queues = dev->data->nb_rx_queues;
	} else {
		queues = adapter->tx_ring;
		nb_queues = dev->data->nb_tx_queues;
	}

	for (i = 0; i < nb_queues; ++i)
		if (queues[i].configured)
			ena_queue_stop(&queues[i]);
}

static int ena_queue_start(struct ena_ring *ring)
{
	int rc, bufs_num;

	ena_assert_msg(ring->configured == 1,
		       "Trying to start unconfigured queue\n");

	rc = ena_create_io_queue(ring);
	if (rc) {
		PMD_INIT_LOG(ERR, "Failed to create IO queue!");
		return rc;
	}

	ring->next_to_clean = 0;
	ring->next_to_use = 0;

	if (ring->type == ENA_RING_TYPE_TX) {
		ring->tx_stats.available_desc =
			ena_com_free_q_entries(ring->ena_com_io_sq);
		return 0;
	}

	bufs_num = ring->ring_size - 1;
	rc = ena_populate_rx_queue(ring, bufs_num);
	if (rc != bufs_num) {
		ena_com_destroy_io_queue(&ring->adapter->ena_dev,
					 ENA_IO_RXQ_IDX(ring->id));
		PMD_INIT_LOG(ERR, "Failed to populate rx ring !");
		return ENA_COM_FAULT;
	}
	/* Flush per-core RX buffers pools cache as they can be used on other
	 * cores as well.
	 */
	rte_mempool_cache_flush(NULL, ring->mb_pool);

	return 0;
}

static int ena_tx_queue_setup(struct rte_eth_dev *dev,
			      uint16_t queue_idx,
			      uint16_t nb_desc,
			      unsigned int socket_id,
			      const struct rte_eth_txconf *tx_conf)
{
	struct ena_ring *txq = NULL;
	struct ena_adapter *adapter = dev->data->dev_private;
	unsigned int i;

	txq = &adapter->tx_ring[queue_idx];

	if (txq->configured) {
		PMD_DRV_LOG(CRIT,
			"API violation. Queue %d is already configured\n",
			queue_idx);
		return ENA_COM_FAULT;
	}

	if (!rte_is_power_of_2(nb_desc)) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of TX queue: %d is not a power of 2.\n",
			nb_desc);
		return -EINVAL;
	}

	if (nb_desc > adapter->max_tx_ring_size) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of TX queue (max size: %d)\n",
			adapter->max_tx_ring_size);
		return -EINVAL;
	}

	txq->port_id = dev->data->port_id;
	txq->next_to_clean = 0;
	txq->next_to_use = 0;
	txq->ring_size = nb_desc;
	txq->size_mask = nb_desc - 1;
	txq->numa_socket_id = socket_id;
	txq->pkts_without_db = false;

	txq->tx_buffer_info = rte_zmalloc("txq->tx_buffer_info",
					  sizeof(struct ena_tx_buffer) *
					  txq->ring_size,
					  RTE_CACHE_LINE_SIZE);
	if (!txq->tx_buffer_info) {
		PMD_DRV_LOG(ERR, "failed to alloc mem for tx buffer info\n");
		return -ENOMEM;
	}

	txq->empty_tx_reqs = rte_zmalloc("txq->empty_tx_reqs",
					 sizeof(u16) * txq->ring_size,
					 RTE_CACHE_LINE_SIZE);
	if (!txq->empty_tx_reqs) {
		PMD_DRV_LOG(ERR, "failed to alloc mem for tx reqs\n");
		rte_free(txq->tx_buffer_info);
		return -ENOMEM;
	}

	txq->push_buf_intermediate_buf =
		rte_zmalloc("txq->push_buf_intermediate_buf",
			    txq->tx_max_header_size,
			    RTE_CACHE_LINE_SIZE);
	if (!txq->push_buf_intermediate_buf) {
		PMD_DRV_LOG(ERR, "failed to alloc push buff for LLQ\n");
		rte_free(txq->tx_buffer_info);
		rte_free(txq->empty_tx_reqs);
		return -ENOMEM;
	}

	for (i = 0; i < txq->ring_size; i++)
		txq->empty_tx_reqs[i] = i;

	if (tx_conf != NULL) {
		txq->offloads =
			tx_conf->offloads | dev->data->dev_conf.txmode.offloads;
	}
	/* Store pointer to this queue in upper layer */
	txq->configured = 1;
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

static int ena_rx_queue_setup(struct rte_eth_dev *dev,
			      uint16_t queue_idx,
			      uint16_t nb_desc,
			      unsigned int socket_id,
			      __rte_unused const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mp)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_ring *rxq = NULL;
	size_t buffer_size;
	int i;

	rxq = &adapter->rx_ring[queue_idx];
	if (rxq->configured) {
		PMD_DRV_LOG(CRIT,
			"API violation. Queue %d is already configured\n",
			queue_idx);
		return ENA_COM_FAULT;
	}

	if (!rte_is_power_of_2(nb_desc)) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of RX queue: %d is not a power of 2.\n",
			nb_desc);
		return -EINVAL;
	}

	if (nb_desc > adapter->max_rx_ring_size) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of RX queue (max size: %d)\n",
			adapter->max_rx_ring_size);
		return -EINVAL;
	}

	/* ENA isn't supporting buffers smaller than 1400 bytes */
	buffer_size = rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM;
	if (buffer_size < ENA_RX_BUF_MIN_SIZE) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of RX buffer: %zu (min size: %d)\n",
			buffer_size, ENA_RX_BUF_MIN_SIZE);
		return -EINVAL;
	}

	rxq->port_id = dev->data->port_id;
	rxq->next_to_clean = 0;
	rxq->next_to_use = 0;
	rxq->ring_size = nb_desc;
	rxq->size_mask = nb_desc - 1;
	rxq->numa_socket_id = socket_id;
	rxq->mb_pool = mp;

	rxq->rx_buffer_info = rte_zmalloc("rxq->buffer_info",
		sizeof(struct ena_rx_buffer) * nb_desc,
		RTE_CACHE_LINE_SIZE);
	if (!rxq->rx_buffer_info) {
		PMD_DRV_LOG(ERR, "failed to alloc mem for rx buffer info\n");
		return -ENOMEM;
	}

	rxq->rx_refill_buffer = rte_zmalloc("rxq->rx_refill_buffer",
					    sizeof(struct rte_mbuf *) * nb_desc,
					    RTE_CACHE_LINE_SIZE);

	if (!rxq->rx_refill_buffer) {
		PMD_DRV_LOG(ERR, "failed to alloc mem for rx refill buffer\n");
		rte_free(rxq->rx_buffer_info);
		rxq->rx_buffer_info = NULL;
		return -ENOMEM;
	}

	rxq->empty_rx_reqs = rte_zmalloc("rxq->empty_rx_reqs",
					 sizeof(uint16_t) * nb_desc,
					 RTE_CACHE_LINE_SIZE);
	if (!rxq->empty_rx_reqs) {
		PMD_DRV_LOG(ERR, "failed to alloc mem for empty rx reqs\n");
		rte_free(rxq->rx_buffer_info);
		rxq->rx_buffer_info = NULL;
		rte_free(rxq->rx_refill_buffer);
		rxq->rx_refill_buffer = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < nb_desc; i++)
		rxq->empty_rx_reqs[i] = i;

	/* Store pointer to this queue in upper layer */
	rxq->configured = 1;
	dev->data->rx_queues[queue_idx] = rxq;

	return 0;
}

static int ena_add_single_rx_desc(struct ena_com_io_sq *io_sq,
				  struct rte_mbuf *mbuf, uint16_t id)
{
	struct ena_com_buf ebuf;
	int rc;

	/* prepare physical address for DMA transaction */
	ebuf.paddr = mbuf->buf_iova + RTE_PKTMBUF_HEADROOM;
	ebuf.len = mbuf->buf_len - RTE_PKTMBUF_HEADROOM;

	/* pass resource to device */
	rc = ena_com_add_single_rx_desc(io_sq, &ebuf, id);
	if (unlikely(rc != 0))
		PMD_DRV_LOG(WARNING, "failed adding rx desc\n");

	return rc;
}

static int ena_populate_rx_queue(struct ena_ring *rxq, unsigned int count)
{
	unsigned int i;
	int rc;
	uint16_t next_to_use = rxq->next_to_use;
	uint16_t in_use, req_id;
	struct rte_mbuf **mbufs = rxq->rx_refill_buffer;

	if (unlikely(!count))
		return 0;

	in_use = rxq->ring_size - 1 -
		ena_com_free_q_entries(rxq->ena_com_io_sq);
	ena_assert_msg(((in_use + count) < rxq->ring_size),
		"bad ring state\n");

	/* get resources for incoming packets */
	rc = rte_pktmbuf_alloc_bulk(rxq->mb_pool, mbufs, count);
	if (unlikely(rc < 0)) {
		rte_atomic64_inc(&rxq->adapter->drv_stats->rx_nombuf);
		++rxq->rx_stats.mbuf_alloc_fail;
		PMD_RX_LOG(DEBUG, "there are no enough free buffers");
		return 0;
	}

	for (i = 0; i < count; i++) {
		struct rte_mbuf *mbuf = mbufs[i];
		struct ena_rx_buffer *rx_info;

		if (likely((i + 4) < count))
			rte_prefetch0(mbufs[i + 4]);

		req_id = rxq->empty_rx_reqs[next_to_use];
		rx_info = &rxq->rx_buffer_info[req_id];

		rc = ena_add_single_rx_desc(rxq->ena_com_io_sq, mbuf, req_id);
		if (unlikely(rc != 0))
			break;

		rx_info->mbuf = mbuf;
		next_to_use = ENA_IDX_NEXT_MASKED(next_to_use, rxq->size_mask);
	}

	if (unlikely(i < count)) {
		PMD_DRV_LOG(WARNING, "refilled rx qid %d with only %d "
			"buffers (from %d)\n", rxq->id, i, count);
		rte_pktmbuf_free_bulk(&mbufs[i], count - i);
		++rxq->rx_stats.refill_partial;
	}

	/* When we submitted free recources to device... */
	if (likely(i > 0)) {
		/* ...let HW know that it can fill buffers with data. */
		ena_com_write_sq_doorbell(rxq->ena_com_io_sq);

		rxq->next_to_use = next_to_use;
	}

	return i;
}

static int ena_device_init(struct ena_com_dev *ena_dev,
			   struct ena_com_dev_get_features_ctx *get_feat_ctx,
			   bool *wd_state)
{
	uint32_t aenq_groups;
	int rc;
	bool readless_supported;

	/* Initialize mmio registers */
	rc = ena_com_mmio_reg_read_request_init(ena_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "failed to init mmio read less\n");
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
	rc = ena_com_dev_reset(ena_dev, ENA_REGS_RESET_NORMAL);
	if (rc) {
		PMD_DRV_LOG(ERR, "cannot reset device\n");
		goto err_mmio_read_less;
	}

	/* check FW version */
	rc = ena_com_validate_version(ena_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "device version is too low\n");
		goto err_mmio_read_less;
	}

	ena_dev->dma_addr_bits = ena_com_get_dma_width(ena_dev);

	/* ENA device administration layer init */
	rc = ena_com_admin_init(ena_dev, &aenq_handlers);
	if (rc) {
		PMD_DRV_LOG(ERR,
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
		PMD_DRV_LOG(ERR,
			"cannot get attribute for ena device rc= %d\n", rc);
		goto err_admin_init;
	}

	aenq_groups = BIT(ENA_ADMIN_LINK_CHANGE) |
		      BIT(ENA_ADMIN_NOTIFICATION) |
		      BIT(ENA_ADMIN_KEEP_ALIVE) |
		      BIT(ENA_ADMIN_FATAL_ERROR) |
		      BIT(ENA_ADMIN_WARNING);

	aenq_groups &= get_feat_ctx->aenq.supported_groups;
	rc = ena_com_set_aenq_config(ena_dev, aenq_groups);
	if (rc) {
		PMD_DRV_LOG(ERR, "Cannot configure aenq groups rc: %d\n", rc);
		goto err_admin_init;
	}

	*wd_state = !!(aenq_groups & BIT(ENA_ADMIN_KEEP_ALIVE));

	return 0;

err_admin_init:
	ena_com_admin_destroy(ena_dev);

err_mmio_read_less:
	ena_com_mmio_reg_read_request_destroy(ena_dev);

	return rc;
}

static void ena_interrupt_handler_rte(void *cb_arg)
{
	struct ena_adapter *adapter = cb_arg;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;

	ena_com_admin_q_comp_intr_handler(ena_dev);
	if (likely(adapter->state != ENA_ADAPTER_STATE_CLOSED))
		ena_com_aenq_intr_handler(ena_dev, adapter);
}

static void check_for_missing_keep_alive(struct ena_adapter *adapter)
{
	if (!adapter->wd_state)
		return;

	if (adapter->keep_alive_timeout == ENA_HW_HINTS_NO_TIMEOUT)
		return;

	if (unlikely((rte_get_timer_cycles() - adapter->timestamp_wd) >=
	    adapter->keep_alive_timeout)) {
		PMD_DRV_LOG(ERR, "Keep alive timeout\n");
		adapter->reset_reason = ENA_REGS_RESET_KEEP_ALIVE_TO;
		adapter->trigger_reset = true;
		++adapter->dev_stats.wd_expired;
	}
}

/* Check if admin queue is enabled */
static void check_for_admin_com_state(struct ena_adapter *adapter)
{
	if (unlikely(!ena_com_get_admin_running_state(&adapter->ena_dev))) {
		PMD_DRV_LOG(ERR, "ENA admin queue is not in running state!\n");
		adapter->reset_reason = ENA_REGS_RESET_ADMIN_TO;
		adapter->trigger_reset = true;
	}
}

static void ena_timer_wd_callback(__rte_unused struct rte_timer *timer,
				  void *arg)
{
	struct ena_adapter *adapter = arg;
	struct rte_eth_dev *dev = adapter->rte_dev;

	check_for_missing_keep_alive(adapter);
	check_for_admin_com_state(adapter);

	if (unlikely(adapter->trigger_reset)) {
		PMD_DRV_LOG(ERR, "Trigger reset is on\n");
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RESET,
			NULL);
	}
}

static inline void
set_default_llq_configurations(struct ena_llq_configurations *llq_config,
			       struct ena_admin_feature_llq_desc *llq,
			       bool use_large_llq_hdr)
{
	llq_config->llq_header_location = ENA_ADMIN_INLINE_HEADER;
	llq_config->llq_stride_ctrl = ENA_ADMIN_MULTIPLE_DESCS_PER_ENTRY;
	llq_config->llq_num_decs_before_header =
		ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_2;

	if (use_large_llq_hdr &&
	    (llq->entry_size_ctrl_supported & ENA_ADMIN_LIST_ENTRY_SIZE_256B)) {
		llq_config->llq_ring_entry_size =
			ENA_ADMIN_LIST_ENTRY_SIZE_256B;
		llq_config->llq_ring_entry_size_value = 256;
	} else {
		llq_config->llq_ring_entry_size =
			ENA_ADMIN_LIST_ENTRY_SIZE_128B;
		llq_config->llq_ring_entry_size_value = 128;
	}
}

static int
ena_set_queues_placement_policy(struct ena_adapter *adapter,
				struct ena_com_dev *ena_dev,
				struct ena_admin_feature_llq_desc *llq,
				struct ena_llq_configurations *llq_default_configurations)
{
	int rc;
	u32 llq_feature_mask;

	llq_feature_mask = 1 << ENA_ADMIN_LLQ;
	if (!(ena_dev->supported_features & llq_feature_mask)) {
		PMD_DRV_LOG(INFO,
			"LLQ is not supported. Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	rc = ena_com_config_dev_mode(ena_dev, llq, llq_default_configurations);
	if (unlikely(rc)) {
		PMD_INIT_LOG(WARNING, "Failed to config dev mode. "
			"Fallback to host mode policy.");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	/* Nothing to config, exit */
	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST)
		return 0;

	if (!adapter->dev_mem_base) {
		PMD_DRV_LOG(ERR, "Unable to access LLQ bar resource. "
			"Fallback to host mode policy.\n.");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	ena_dev->mem_bar = adapter->dev_mem_base;

	return 0;
}

static uint32_t ena_calc_max_io_queue_num(struct ena_com_dev *ena_dev,
	struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	uint32_t io_tx_sq_num, io_tx_cq_num, io_rx_num, max_num_io_queues;

	/* Regular queues capabilities */
	if (ena_dev->supported_features & BIT(ENA_ADMIN_MAX_QUEUES_EXT)) {
		struct ena_admin_queue_ext_feature_fields *max_queue_ext =
			&get_feat_ctx->max_queue_ext.max_queue_ext;
		io_rx_num = RTE_MIN(max_queue_ext->max_rx_sq_num,
				    max_queue_ext->max_rx_cq_num);
		io_tx_sq_num = max_queue_ext->max_tx_sq_num;
		io_tx_cq_num = max_queue_ext->max_tx_cq_num;
	} else {
		struct ena_admin_queue_feature_desc *max_queues =
			&get_feat_ctx->max_queues;
		io_tx_sq_num = max_queues->max_sq_num;
		io_tx_cq_num = max_queues->max_cq_num;
		io_rx_num = RTE_MIN(io_tx_sq_num, io_tx_cq_num);
	}

	/* In case of LLQ use the llq number in the get feature cmd */
	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV)
		io_tx_sq_num = get_feat_ctx->llq.max_llq_num;

	max_num_io_queues = RTE_MIN(ENA_MAX_NUM_IO_QUEUES, io_rx_num);
	max_num_io_queues = RTE_MIN(max_num_io_queues, io_tx_sq_num);
	max_num_io_queues = RTE_MIN(max_num_io_queues, io_tx_cq_num);

	if (unlikely(max_num_io_queues == 0)) {
		PMD_DRV_LOG(ERR, "Number of IO queues should not be 0\n");
		return -EFAULT;
	}

	return max_num_io_queues;
}

static void
ena_set_offloads(struct ena_offloads *offloads,
		 struct ena_admin_feature_offload_desc *offload_desc)
{
	if (offload_desc->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK)
		offloads->tx_offloads |= ENA_IPV4_TSO;

	/* Tx IPv4 checksum offloads */
	if (offload_desc->tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK)
		offloads->tx_offloads |= ENA_L3_IPV4_CSUM;
	if (offload_desc->tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK)
		offloads->tx_offloads |= ENA_L4_IPV4_CSUM;
	if (offload_desc->tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK)
		offloads->tx_offloads |= ENA_L4_IPV4_CSUM_PARTIAL;

	/* Tx IPv6 checksum offloads */
	if (offload_desc->tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK)
		offloads->tx_offloads |= ENA_L4_IPV6_CSUM;
	if (offload_desc->tx &
	     ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK)
		offloads->tx_offloads |= ENA_L4_IPV6_CSUM_PARTIAL;

	/* Rx IPv4 checksum offloads */
	if (offload_desc->rx_supported &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK)
		offloads->rx_offloads |= ENA_L3_IPV4_CSUM;
	if (offload_desc->rx_supported &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK)
		offloads->rx_offloads |= ENA_L4_IPV4_CSUM;

	/* Rx IPv6 checksum offloads */
	if (offload_desc->rx_supported &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK)
		offloads->rx_offloads |= ENA_L4_IPV6_CSUM;

	if (offload_desc->rx_supported &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK)
		offloads->rx_offloads |= ENA_RX_RSS_HASH;
}

static int eth_ena_dev_init(struct rte_eth_dev *eth_dev)
{
	struct ena_calc_queue_size_ctx calc_queue_ctx = { 0 };
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
	struct ena_adapter *adapter = eth_dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	struct ena_com_dev_get_features_ctx get_feat_ctx;
	struct ena_llq_configurations llq_config;
	const char *queue_type_str;
	uint32_t max_num_io_queues;
	int rc;
	static int adapters_found;
	bool disable_meta_caching;
	bool wd_state = false;

	eth_dev->dev_ops = &ena_dev_ops;
	eth_dev->rx_pkt_burst = &eth_ena_recv_pkts;
	eth_dev->tx_pkt_burst = &eth_ena_xmit_pkts;
	eth_dev->tx_pkt_prepare = &eth_ena_prep_pkts;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	memset(adapter, 0, sizeof(struct ena_adapter));
	ena_dev = &adapter->ena_dev;

	adapter->rte_eth_dev_data = eth_dev->data;
	adapter->rte_dev = eth_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	adapter->pdev = pci_dev;

	PMD_INIT_LOG(INFO, "Initializing %x:%x:%x.%d",
		     pci_dev->addr.domain,
		     pci_dev->addr.bus,
		     pci_dev->addr.devid,
		     pci_dev->addr.function);

	intr_handle = &pci_dev->intr_handle;

	adapter->regs = pci_dev->mem_resource[ENA_REGS_BAR].addr;
	adapter->dev_mem_base = pci_dev->mem_resource[ENA_MEM_BAR].addr;

	if (!adapter->regs) {
		PMD_INIT_LOG(CRIT, "Failed to access registers BAR(%d)",
			     ENA_REGS_BAR);
		return -ENXIO;
	}

	ena_dev->reg_bar = adapter->regs;
	ena_dev->dmadev = adapter->pdev;

	adapter->id_number = adapters_found;

	snprintf(adapter->name, ENA_NAME_MAX_LEN, "ena_%d",
		 adapter->id_number);

	rc = ena_parse_devargs(adapter, pci_dev->device.devargs);
	if (rc != 0) {
		PMD_INIT_LOG(CRIT, "Failed to parse devargs\n");
		goto err;
	}

	/* device specific initialization routine */
	rc = ena_device_init(ena_dev, &get_feat_ctx, &wd_state);
	if (rc) {
		PMD_INIT_LOG(CRIT, "Failed to init ENA device");
		goto err;
	}
	adapter->wd_state = wd_state;

	set_default_llq_configurations(&llq_config, &get_feat_ctx.llq,
		adapter->use_large_llq_hdr);
	rc = ena_set_queues_placement_policy(adapter, ena_dev,
					     &get_feat_ctx.llq, &llq_config);
	if (unlikely(rc)) {
		PMD_INIT_LOG(CRIT, "Failed to set placement policy");
		return rc;
	}

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST)
		queue_type_str = "Regular";
	else
		queue_type_str = "Low latency";
	PMD_DRV_LOG(INFO, "Placement policy: %s\n", queue_type_str);

	calc_queue_ctx.ena_dev = ena_dev;
	calc_queue_ctx.get_feat_ctx = &get_feat_ctx;

	max_num_io_queues = ena_calc_max_io_queue_num(ena_dev, &get_feat_ctx);
	rc = ena_calc_io_queue_size(&calc_queue_ctx,
		adapter->use_large_llq_hdr);
	if (unlikely((rc != 0) || (max_num_io_queues == 0))) {
		rc = -EFAULT;
		goto err_device_destroy;
	}

	adapter->max_tx_ring_size = calc_queue_ctx.max_tx_queue_size;
	adapter->max_rx_ring_size = calc_queue_ctx.max_rx_queue_size;
	adapter->max_tx_sgl_size = calc_queue_ctx.max_tx_sgl_size;
	adapter->max_rx_sgl_size = calc_queue_ctx.max_rx_sgl_size;
	adapter->max_num_io_queues = max_num_io_queues;

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV) {
		disable_meta_caching =
			!!(get_feat_ctx.llq.accel_mode.u.get.supported_flags &
			BIT(ENA_ADMIN_DISABLE_META_CACHING));
	} else {
		disable_meta_caching = false;
	}

	/* prepare ring structures */
	ena_init_rings(adapter, disable_meta_caching);

	ena_config_debug_area(adapter);

	/* Set max MTU for this device */
	adapter->max_mtu = get_feat_ctx.dev_attr.max_mtu;

	ena_set_offloads(&adapter->offloads, &get_feat_ctx.offload);

	/* Copy MAC address and point DPDK to it */
	eth_dev->data->mac_addrs = (struct rte_ether_addr *)adapter->mac_addr;
	rte_ether_addr_copy((struct rte_ether_addr *)
			get_feat_ctx.dev_attr.mac_addr,
			(struct rte_ether_addr *)adapter->mac_addr);

	adapter->drv_stats = rte_zmalloc("adapter stats",
					 sizeof(*adapter->drv_stats),
					 RTE_CACHE_LINE_SIZE);
	if (!adapter->drv_stats) {
		PMD_DRV_LOG(ERR, "failed to alloc mem for adapter stats\n");
		rc = -ENOMEM;
		goto err_delete_debug_area;
	}

	rte_spinlock_init(&adapter->admin_lock);

	rte_intr_callback_register(intr_handle,
				   ena_interrupt_handler_rte,
				   adapter);
	rte_intr_enable(intr_handle);
	ena_com_set_admin_polling_mode(ena_dev, false);
	ena_com_admin_aenq_enable(ena_dev);

	if (adapters_found == 0)
		rte_timer_subsystem_init();
	rte_timer_init(&adapter->timer_wd);

	adapters_found++;
	adapter->state = ENA_ADAPTER_STATE_INIT;

	return 0;

err_delete_debug_area:
	ena_com_delete_debug_area(ena_dev);

err_device_destroy:
	ena_com_delete_host_info(ena_dev);
	ena_com_admin_destroy(ena_dev);

err:
	return rc;
}

static void ena_destroy_device(struct rte_eth_dev *eth_dev)
{
	struct ena_adapter *adapter = eth_dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;

	if (adapter->state == ENA_ADAPTER_STATE_FREE)
		return;

	ena_com_set_admin_running_state(ena_dev, false);

	if (adapter->state != ENA_ADAPTER_STATE_CLOSED)
		ena_close(eth_dev);

	ena_com_delete_debug_area(ena_dev);
	ena_com_delete_host_info(ena_dev);

	ena_com_abort_admin_commands(ena_dev);
	ena_com_wait_for_abort_completion(ena_dev);
	ena_com_admin_destroy(ena_dev);
	ena_com_mmio_reg_read_request_destroy(ena_dev);

	adapter->state = ENA_ADAPTER_STATE_FREE;
}

static int eth_ena_dev_uninit(struct rte_eth_dev *eth_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ena_destroy_device(eth_dev);

	return 0;
}

static int ena_dev_configure(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter = dev->data->dev_private;

	adapter->state = ENA_ADAPTER_STATE_CONFIG;

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;
	dev->data->dev_conf.txmode.offloads |= DEV_TX_OFFLOAD_MULTI_SEGS;

	/* Scattered Rx cannot be turned off in the HW, so this capability must
	 * be forced.
	 */
	dev->data->scattered_rx = 1;

	adapter->tx_selected_offloads = dev->data->dev_conf.txmode.offloads;
	adapter->rx_selected_offloads = dev->data->dev_conf.rxmode.offloads;

	return 0;
}

static void ena_init_rings(struct ena_adapter *adapter,
			   bool disable_meta_caching)
{
	size_t i;

	for (i = 0; i < adapter->max_num_io_queues; i++) {
		struct ena_ring *ring = &adapter->tx_ring[i];

		ring->configured = 0;
		ring->type = ENA_RING_TYPE_TX;
		ring->adapter = adapter;
		ring->id = i;
		ring->tx_mem_queue_type = adapter->ena_dev.tx_mem_queue_type;
		ring->tx_max_header_size = adapter->ena_dev.tx_max_header_size;
		ring->sgl_size = adapter->max_tx_sgl_size;
		ring->disable_meta_caching = disable_meta_caching;
	}

	for (i = 0; i < adapter->max_num_io_queues; i++) {
		struct ena_ring *ring = &adapter->rx_ring[i];

		ring->configured = 0;
		ring->type = ENA_RING_TYPE_RX;
		ring->adapter = adapter;
		ring->id = i;
		ring->sgl_size = adapter->max_rx_sgl_size;
	}
}

static uint64_t ena_get_rx_port_offloads(struct ena_adapter *adapter)
{
	uint64_t port_offloads = 0;

	if (adapter->offloads.rx_offloads & ENA_L3_IPV4_CSUM)
		port_offloads |= DEV_RX_OFFLOAD_IPV4_CKSUM;

	if (adapter->offloads.rx_offloads &
	    (ENA_L4_IPV4_CSUM | ENA_L4_IPV6_CSUM))
		port_offloads |=
			DEV_RX_OFFLOAD_UDP_CKSUM | DEV_RX_OFFLOAD_TCP_CKSUM;

	if (adapter->offloads.rx_offloads & ENA_RX_RSS_HASH)
		port_offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	port_offloads |= DEV_RX_OFFLOAD_SCATTER;

	return port_offloads;
}

static uint64_t ena_get_tx_port_offloads(struct ena_adapter *adapter)
{
	uint64_t port_offloads = 0;

	if (adapter->offloads.tx_offloads & ENA_IPV4_TSO)
		port_offloads |= DEV_TX_OFFLOAD_TCP_TSO;

	if (adapter->offloads.tx_offloads & ENA_L3_IPV4_CSUM)
		port_offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
	if (adapter->offloads.tx_offloads &
	    (ENA_L4_IPV4_CSUM_PARTIAL | ENA_L4_IPV4_CSUM |
	     ENA_L4_IPV6_CSUM | ENA_L4_IPV6_CSUM_PARTIAL))
		port_offloads |=
			DEV_TX_OFFLOAD_UDP_CKSUM | DEV_TX_OFFLOAD_TCP_CKSUM;

	port_offloads |= DEV_TX_OFFLOAD_MULTI_SEGS;

	return port_offloads;
}

static uint64_t ena_get_rx_queue_offloads(struct ena_adapter *adapter)
{
	RTE_SET_USED(adapter);

	return 0;
}

static uint64_t ena_get_tx_queue_offloads(struct ena_adapter *adapter)
{
	RTE_SET_USED(adapter);

	return 0;
}

static int ena_infos_get(struct rte_eth_dev *dev,
			  struct rte_eth_dev_info *dev_info)
{
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev;

	ena_assert_msg(dev->data != NULL, "Uninitialized device\n");
	ena_assert_msg(dev->data->dev_private != NULL, "Uninitialized device\n");
	adapter = dev->data->dev_private;

	ena_dev = &adapter->ena_dev;
	ena_assert_msg(ena_dev != NULL, "Uninitialized device\n");

	dev_info->speed_capa =
			ETH_LINK_SPEED_1G   |
			ETH_LINK_SPEED_2_5G |
			ETH_LINK_SPEED_5G   |
			ETH_LINK_SPEED_10G  |
			ETH_LINK_SPEED_25G  |
			ETH_LINK_SPEED_40G  |
			ETH_LINK_SPEED_50G  |
			ETH_LINK_SPEED_100G;

	/* Inform framework about available features */
	dev_info->rx_offload_capa = ena_get_rx_port_offloads(adapter);
	dev_info->tx_offload_capa = ena_get_tx_port_offloads(adapter);
	dev_info->rx_queue_offload_capa = ena_get_rx_queue_offloads(adapter);
	dev_info->tx_queue_offload_capa = ena_get_tx_queue_offloads(adapter);

	dev_info->flow_type_rss_offloads = ETH_RSS_IP | ETH_RSS_TCP |
					   ETH_RSS_UDP;

	dev_info->min_rx_bufsize = ENA_MIN_FRAME_LEN;
	dev_info->max_rx_pktlen  = adapter->max_mtu;
	dev_info->max_mac_addrs = 1;

	dev_info->max_rx_queues = adapter->max_num_io_queues;
	dev_info->max_tx_queues = adapter->max_num_io_queues;
	dev_info->reta_size = ENA_RX_RSS_TABLE_SIZE;

	dev_info->rx_desc_lim.nb_max = adapter->max_rx_ring_size;
	dev_info->rx_desc_lim.nb_min = ENA_MIN_RING_DESC;
	dev_info->rx_desc_lim.nb_seg_max = RTE_MIN(ENA_PKT_MAX_BUFS,
					adapter->max_rx_sgl_size);
	dev_info->rx_desc_lim.nb_mtu_seg_max = RTE_MIN(ENA_PKT_MAX_BUFS,
					adapter->max_rx_sgl_size);

	dev_info->tx_desc_lim.nb_max = adapter->max_tx_ring_size;
	dev_info->tx_desc_lim.nb_min = ENA_MIN_RING_DESC;
	dev_info->tx_desc_lim.nb_seg_max = RTE_MIN(ENA_PKT_MAX_BUFS,
					adapter->max_tx_sgl_size);
	dev_info->tx_desc_lim.nb_mtu_seg_max = RTE_MIN(ENA_PKT_MAX_BUFS,
					adapter->max_tx_sgl_size);

	dev_info->default_rxportconf.ring_size = ENA_DEFAULT_RING_SIZE;
	dev_info->default_txportconf.ring_size = ENA_DEFAULT_RING_SIZE;

	return 0;
}

static inline void ena_init_rx_mbuf(struct rte_mbuf *mbuf, uint16_t len)
{
	mbuf->data_len = len;
	mbuf->data_off = RTE_PKTMBUF_HEADROOM;
	mbuf->refcnt = 1;
	mbuf->next = NULL;
}

static struct rte_mbuf *ena_rx_mbuf(struct ena_ring *rx_ring,
				    struct ena_com_rx_buf_info *ena_bufs,
				    uint32_t descs,
				    uint16_t *next_to_clean,
				    uint8_t offset)
{
	struct rte_mbuf *mbuf;
	struct rte_mbuf *mbuf_head;
	struct ena_rx_buffer *rx_info;
	int rc;
	uint16_t ntc, len, req_id, buf = 0;

	if (unlikely(descs == 0))
		return NULL;

	ntc = *next_to_clean;

	len = ena_bufs[buf].len;
	req_id = ena_bufs[buf].req_id;

	rx_info = &rx_ring->rx_buffer_info[req_id];

	mbuf = rx_info->mbuf;
	RTE_ASSERT(mbuf != NULL);

	ena_init_rx_mbuf(mbuf, len);

	/* Fill the mbuf head with the data specific for 1st segment. */
	mbuf_head = mbuf;
	mbuf_head->nb_segs = descs;
	mbuf_head->port = rx_ring->port_id;
	mbuf_head->pkt_len = len;
	mbuf_head->data_off += offset;

	rx_info->mbuf = NULL;
	rx_ring->empty_rx_reqs[ntc] = req_id;
	ntc = ENA_IDX_NEXT_MASKED(ntc, rx_ring->size_mask);

	while (--descs) {
		++buf;
		len = ena_bufs[buf].len;
		req_id = ena_bufs[buf].req_id;

		rx_info = &rx_ring->rx_buffer_info[req_id];
		RTE_ASSERT(rx_info->mbuf != NULL);

		if (unlikely(len == 0)) {
			/*
			 * Some devices can pass descriptor with the length 0.
			 * To avoid confusion, the PMD is simply putting the
			 * descriptor back, as it was never used. We'll avoid
			 * mbuf allocation that way.
			 */
			rc = ena_add_single_rx_desc(rx_ring->ena_com_io_sq,
				rx_info->mbuf, req_id);
			if (unlikely(rc != 0)) {
				/* Free the mbuf in case of an error. */
				rte_mbuf_raw_free(rx_info->mbuf);
			} else {
				/*
				 * If there was no error, just exit the loop as
				 * 0 length descriptor is always the last one.
				 */
				break;
			}
		} else {
			/* Create an mbuf chain. */
			mbuf->next = rx_info->mbuf;
			mbuf = mbuf->next;

			ena_init_rx_mbuf(mbuf, len);
			mbuf_head->pkt_len += len;
		}

		/*
		 * Mark the descriptor as depleted and perform necessary
		 * cleanup.
		 * This code will execute in two cases:
		 *  1. Descriptor len was greater than 0 - normal situation.
		 *  2. Descriptor len was 0 and we failed to add the descriptor
		 *     to the device. In that situation, we should try to add
		 *     the mbuf again in the populate routine and mark the
		 *     descriptor as used up by the device.
		 */
		rx_info->mbuf = NULL;
		rx_ring->empty_rx_reqs[ntc] = req_id;
		ntc = ENA_IDX_NEXT_MASKED(ntc, rx_ring->size_mask);
	}

	*next_to_clean = ntc;

	return mbuf_head;
}

static uint16_t eth_ena_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	struct ena_ring *rx_ring = (struct ena_ring *)(rx_queue);
	unsigned int free_queue_entries;
	unsigned int refill_threshold;
	uint16_t next_to_clean = rx_ring->next_to_clean;
	uint16_t descs_in_use;
	struct rte_mbuf *mbuf;
	uint16_t completed;
	struct ena_com_rx_ctx ena_rx_ctx;
	int i, rc = 0;

	/* Check adapter state */
	if (unlikely(rx_ring->adapter->state != ENA_ADAPTER_STATE_RUNNING)) {
		PMD_DRV_LOG(ALERT,
			"Trying to receive pkts while device is NOT running\n");
		return 0;
	}

	descs_in_use = rx_ring->ring_size -
		ena_com_free_q_entries(rx_ring->ena_com_io_sq) - 1;
	nb_pkts = RTE_MIN(descs_in_use, nb_pkts);

	for (completed = 0; completed < nb_pkts; completed++) {
		ena_rx_ctx.max_bufs = rx_ring->sgl_size;
		ena_rx_ctx.ena_bufs = rx_ring->ena_bufs;
		ena_rx_ctx.descs = 0;
		ena_rx_ctx.pkt_offset = 0;
		/* receive packet context */
		rc = ena_com_rx_pkt(rx_ring->ena_com_io_cq,
				    rx_ring->ena_com_io_sq,
				    &ena_rx_ctx);
		if (unlikely(rc)) {
			PMD_DRV_LOG(ERR, "ena_com_rx_pkt error %d\n", rc);
			if (rc == ENA_COM_NO_SPACE) {
				++rx_ring->rx_stats.bad_desc_num;
				rx_ring->adapter->reset_reason =
					ENA_REGS_RESET_TOO_MANY_RX_DESCS;
			} else {
				++rx_ring->rx_stats.bad_req_id;
				rx_ring->adapter->reset_reason =
					ENA_REGS_RESET_INV_RX_REQ_ID;
			}
			rx_ring->adapter->trigger_reset = true;
			return 0;
		}

		mbuf = ena_rx_mbuf(rx_ring,
			ena_rx_ctx.ena_bufs,
			ena_rx_ctx.descs,
			&next_to_clean,
			ena_rx_ctx.pkt_offset);
		if (unlikely(mbuf == NULL)) {
			for (i = 0; i < ena_rx_ctx.descs; ++i) {
				rx_ring->empty_rx_reqs[next_to_clean] =
					rx_ring->ena_bufs[i].req_id;
				next_to_clean = ENA_IDX_NEXT_MASKED(
					next_to_clean, rx_ring->size_mask);
			}
			break;
		}

		/* fill mbuf attributes if any */
		ena_rx_mbuf_prepare(mbuf, &ena_rx_ctx);

		if (unlikely(mbuf->ol_flags &
				(PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD))) {
			rte_atomic64_inc(&rx_ring->adapter->drv_stats->ierrors);
			++rx_ring->rx_stats.bad_csum;
		}

		rx_pkts[completed] = mbuf;
		rx_ring->rx_stats.bytes += mbuf->pkt_len;
	}

	rx_ring->rx_stats.cnt += completed;
	rx_ring->next_to_clean = next_to_clean;

	free_queue_entries = ena_com_free_q_entries(rx_ring->ena_com_io_sq);
	refill_threshold =
		RTE_MIN(rx_ring->ring_size / ENA_REFILL_THRESH_DIVIDER,
		(unsigned int)ENA_REFILL_THRESH_PACKET);

	/* Burst refill to save doorbells, memory barriers, const interval */
	if (free_queue_entries > refill_threshold) {
		ena_com_update_dev_comp_head(rx_ring->ena_com_io_cq);
		ena_populate_rx_queue(rx_ring, free_queue_entries);
	}

	return completed;
}

static uint16_t
eth_ena_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int32_t ret;
	uint32_t i;
	struct rte_mbuf *m;
	struct ena_ring *tx_ring = (struct ena_ring *)(tx_queue);
	struct ena_adapter *adapter = tx_ring->adapter;
	struct rte_ipv4_hdr *ip_hdr;
	uint64_t ol_flags;
	uint64_t l4_csum_flag;
	uint64_t dev_offload_capa;
	uint16_t frag_field;
	bool need_pseudo_csum;

	dev_offload_capa = adapter->offloads.tx_offloads;
	for (i = 0; i != nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/* Check if any offload flag was set */
		if (ol_flags == 0)
			continue;

		l4_csum_flag = ol_flags & PKT_TX_L4_MASK;
		/* SCTP checksum offload is not supported by the ENA. */
		if ((ol_flags & ENA_TX_OFFLOAD_NOTSUP_MASK) ||
		    l4_csum_flag == PKT_TX_SCTP_CKSUM) {
			PMD_TX_LOG(DEBUG,
				"mbuf[%" PRIu32 "] has unsupported offloads flags set: 0x%" PRIu64 "\n",
				i, ol_flags);
			rte_errno = ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		/* Check if requested offload is also enabled for the queue */
		if ((ol_flags & PKT_TX_IP_CKSUM &&
		     !(tx_ring->offloads & DEV_TX_OFFLOAD_IPV4_CKSUM)) ||
		    (l4_csum_flag == PKT_TX_TCP_CKSUM &&
		     !(tx_ring->offloads & DEV_TX_OFFLOAD_TCP_CKSUM)) ||
		    (l4_csum_flag == PKT_TX_UDP_CKSUM &&
		     !(tx_ring->offloads & DEV_TX_OFFLOAD_UDP_CKSUM))) {
			PMD_TX_LOG(DEBUG,
				"mbuf[%" PRIu32 "]: requested offloads: %" PRIu16 " are not enabled for the queue[%u]\n",
				i, m->nb_segs, tx_ring->id);
			rte_errno = EINVAL;
			return i;
		}

		/* The caller is obligated to set l2 and l3 len if any cksum
		 * offload is enabled.
		 */
		if (unlikely(ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_L4_MASK) &&
		    (m->l2_len == 0 || m->l3_len == 0))) {
			PMD_TX_LOG(DEBUG,
				"mbuf[%" PRIu32 "]: l2_len or l3_len values are 0 while the offload was requested\n",
				i);
			rte_errno = EINVAL;
			return i;
		}
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif

		/* Verify HW support for requested offloads and determine if
		 * pseudo header checksum is needed.
		 */
		need_pseudo_csum = false;
		if (ol_flags & PKT_TX_IPV4) {
			if (ol_flags & PKT_TX_IP_CKSUM &&
			    !(dev_offload_capa & ENA_L3_IPV4_CSUM)) {
				rte_errno = ENOTSUP;
				return i;
			}

			if (ol_flags & PKT_TX_TCP_SEG &&
			    !(dev_offload_capa & ENA_IPV4_TSO)) {
				rte_errno = ENOTSUP;
				return i;
			}

			/* Check HW capabilities and if pseudo csum is needed
			 * for L4 offloads.
			 */
			if (l4_csum_flag != PKT_TX_L4_NO_CKSUM &&
			    !(dev_offload_capa & ENA_L4_IPV4_CSUM)) {
				if (dev_offload_capa &
				    ENA_L4_IPV4_CSUM_PARTIAL) {
					need_pseudo_csum = true;
				} else {
					rte_errno = ENOTSUP;
					return i;
				}
			}

			/* Parse the DF flag */
			ip_hdr = rte_pktmbuf_mtod_offset(m,
				struct rte_ipv4_hdr *, m->l2_len);
			frag_field = rte_be_to_cpu_16(ip_hdr->fragment_offset);
			if (frag_field & RTE_IPV4_HDR_DF_FLAG) {
				m->packet_type |= RTE_PTYPE_L4_NONFRAG;
			} else if (ol_flags & PKT_TX_TCP_SEG) {
				/* In case we are supposed to TSO and have DF
				 * not set (DF=0) hardware must be provided with
				 * partial checksum.
				 */
				need_pseudo_csum = true;
			}
		} else if (ol_flags & PKT_TX_IPV6) {
			/* There is no support for IPv6 TSO as for now. */
			if (ol_flags & PKT_TX_TCP_SEG) {
				rte_errno = ENOTSUP;
				return i;
			}

			/* Check HW capabilities and if pseudo csum is needed */
			if (l4_csum_flag != PKT_TX_L4_NO_CKSUM &&
			    !(dev_offload_capa & ENA_L4_IPV6_CSUM)) {
				if (dev_offload_capa &
				    ENA_L4_IPV6_CSUM_PARTIAL) {
					need_pseudo_csum = true;
				} else {
					rte_errno = ENOTSUP;
					return i;
				}
			}
		}

		if (need_pseudo_csum) {
			ret = rte_net_intel_cksum_flags_prepare(m, ol_flags);
			if (ret != 0) {
				rte_errno = -ret;
				return i;
			}
		}
	}

	return i;
}

static void ena_update_hints(struct ena_adapter *adapter,
			     struct ena_admin_ena_hw_hints *hints)
{
	if (hints->admin_completion_tx_timeout)
		adapter->ena_dev.admin_queue.completion_timeout =
			hints->admin_completion_tx_timeout * 1000;

	if (hints->mmio_read_timeout)
		/* convert to usec */
		adapter->ena_dev.mmio_read.reg_read_to =
			hints->mmio_read_timeout * 1000;

	if (hints->driver_watchdog_timeout) {
		if (hints->driver_watchdog_timeout == ENA_HW_HINTS_NO_TIMEOUT)
			adapter->keep_alive_timeout = ENA_HW_HINTS_NO_TIMEOUT;
		else
			// Convert msecs to ticks
			adapter->keep_alive_timeout =
				(hints->driver_watchdog_timeout *
				rte_get_timer_hz()) / 1000;
	}
}

static int ena_check_space_and_linearize_mbuf(struct ena_ring *tx_ring,
					      struct rte_mbuf *mbuf)
{
	struct ena_com_dev *ena_dev;
	int num_segments, header_len, rc;

	ena_dev = &tx_ring->adapter->ena_dev;
	num_segments = mbuf->nb_segs;
	header_len = mbuf->data_len;

	if (likely(num_segments < tx_ring->sgl_size))
		goto checkspace;

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV &&
	    (num_segments == tx_ring->sgl_size) &&
	    (header_len < tx_ring->tx_max_header_size))
		goto checkspace;

	/* Checking for space for 2 additional metadata descriptors due to
	 * possible header split and metadata descriptor. Linearization will
	 * be needed so we reduce the segments number from num_segments to 1
	 */
	if (!ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq, 3)) {
		PMD_DRV_LOG(DEBUG, "Not enough space in the tx queue\n");
		return ENA_COM_NO_MEM;
	}
	++tx_ring->tx_stats.linearize;
	rc = rte_pktmbuf_linearize(mbuf);
	if (unlikely(rc)) {
		PMD_DRV_LOG(WARNING, "Mbuf linearize failed\n");
		rte_atomic64_inc(&tx_ring->adapter->drv_stats->ierrors);
		++tx_ring->tx_stats.linearize_failed;
		return rc;
	}

	return 0;

checkspace:
	/* Checking for space for 2 additional metadata descriptors due to
	 * possible header split and metadata descriptor
	 */
	if (!ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
					  num_segments + 2)) {
		PMD_DRV_LOG(DEBUG, "Not enough space in the tx queue\n");
		return ENA_COM_NO_MEM;
	}

	return 0;
}

static void ena_tx_map_mbuf(struct ena_ring *tx_ring,
	struct ena_tx_buffer *tx_info,
	struct rte_mbuf *mbuf,
	void **push_header,
	uint16_t *header_len)
{
	struct ena_com_buf *ena_buf;
	uint16_t delta, seg_len, push_len;

	delta = 0;
	seg_len = mbuf->data_len;

	tx_info->mbuf = mbuf;
	ena_buf = tx_info->bufs;

	if (tx_ring->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV) {
		/*
		 * Tx header might be (and will be in most cases) smaller than
		 * tx_max_header_size. But it's not an issue to send more data
		 * to the device, than actually needed if the mbuf size is
		 * greater than tx_max_header_size.
		 */
		push_len = RTE_MIN(mbuf->pkt_len, tx_ring->tx_max_header_size);
		*header_len = push_len;

		if (likely(push_len <= seg_len)) {
			/* If the push header is in the single segment, then
			 * just point it to the 1st mbuf data.
			 */
			*push_header = rte_pktmbuf_mtod(mbuf, uint8_t *);
		} else {
			/* If the push header lays in the several segments, copy
			 * it to the intermediate buffer.
			 */
			rte_pktmbuf_read(mbuf, 0, push_len,
				tx_ring->push_buf_intermediate_buf);
			*push_header = tx_ring->push_buf_intermediate_buf;
			delta = push_len - seg_len;
		}
	} else {
		*push_header = NULL;
		*header_len = 0;
		push_len = 0;
	}

	/* Process first segment taking into consideration pushed header */
	if (seg_len > push_len) {
		ena_buf->paddr = mbuf->buf_iova +
				mbuf->data_off +
				push_len;
		ena_buf->len = seg_len - push_len;
		ena_buf++;
		tx_info->num_of_bufs++;
	}

	while ((mbuf = mbuf->next) != NULL) {
		seg_len = mbuf->data_len;

		/* Skip mbufs if whole data is pushed as a header */
		if (unlikely(delta > seg_len)) {
			delta -= seg_len;
			continue;
		}

		ena_buf->paddr = mbuf->buf_iova + mbuf->data_off + delta;
		ena_buf->len = seg_len - delta;
		ena_buf++;
		tx_info->num_of_bufs++;

		delta = 0;
	}
}

static int ena_xmit_mbuf(struct ena_ring *tx_ring, struct rte_mbuf *mbuf)
{
	struct ena_tx_buffer *tx_info;
	struct ena_com_tx_ctx ena_tx_ctx = { { 0 } };
	uint16_t next_to_use;
	uint16_t header_len;
	uint16_t req_id;
	void *push_header;
	int nb_hw_desc;
	int rc;

	rc = ena_check_space_and_linearize_mbuf(tx_ring, mbuf);
	if (unlikely(rc))
		return rc;

	next_to_use = tx_ring->next_to_use;

	req_id = tx_ring->empty_tx_reqs[next_to_use];
	tx_info = &tx_ring->tx_buffer_info[req_id];
	tx_info->num_of_bufs = 0;

	ena_tx_map_mbuf(tx_ring, tx_info, mbuf, &push_header, &header_len);

	ena_tx_ctx.ena_bufs = tx_info->bufs;
	ena_tx_ctx.push_header = push_header;
	ena_tx_ctx.num_bufs = tx_info->num_of_bufs;
	ena_tx_ctx.req_id = req_id;
	ena_tx_ctx.header_len = header_len;

	/* Set Tx offloads flags, if applicable */
	ena_tx_mbuf_prepare(mbuf, &ena_tx_ctx, tx_ring->offloads,
		tx_ring->disable_meta_caching);

	if (unlikely(ena_com_is_doorbell_needed(tx_ring->ena_com_io_sq,
			&ena_tx_ctx))) {
		PMD_DRV_LOG(DEBUG,
			"llq tx max burst size of queue %d achieved, writing doorbell to send burst\n",
			tx_ring->id);
		ena_com_write_sq_doorbell(tx_ring->ena_com_io_sq);
		tx_ring->tx_stats.doorbells++;
		tx_ring->pkts_without_db = false;
	}

	/* prepare the packet's descriptors to dma engine */
	rc = ena_com_prepare_tx(tx_ring->ena_com_io_sq,	&ena_tx_ctx,
		&nb_hw_desc);
	if (unlikely(rc)) {
		PMD_DRV_LOG(ERR, "Failed to prepare Tx buffers, rc: %d\n", rc);
		++tx_ring->tx_stats.prepare_ctx_err;
		tx_ring->adapter->reset_reason =
		    ENA_REGS_RESET_DRIVER_INVALID_STATE;
		tx_ring->adapter->trigger_reset = true;
		return rc;
	}

	tx_info->tx_descs = nb_hw_desc;

	tx_ring->tx_stats.cnt++;
	tx_ring->tx_stats.bytes += mbuf->pkt_len;

	tx_ring->next_to_use = ENA_IDX_NEXT_MASKED(next_to_use,
		tx_ring->size_mask);

	return 0;
}

static void ena_tx_cleanup(struct ena_ring *tx_ring)
{
	unsigned int cleanup_budget;
	unsigned int total_tx_descs = 0;
	uint16_t next_to_clean = tx_ring->next_to_clean;

	cleanup_budget = RTE_MIN(tx_ring->ring_size / ENA_REFILL_THRESH_DIVIDER,
		(unsigned int)ENA_REFILL_THRESH_PACKET);

	while (likely(total_tx_descs < cleanup_budget)) {
		struct rte_mbuf *mbuf;
		struct ena_tx_buffer *tx_info;
		uint16_t req_id;

		if (ena_com_tx_comp_req_id_get(tx_ring->ena_com_io_cq, &req_id) != 0)
			break;

		if (unlikely(validate_tx_req_id(tx_ring, req_id) != 0))
			break;

		/* Get Tx info & store how many descs were processed  */
		tx_info = &tx_ring->tx_buffer_info[req_id];

		mbuf = tx_info->mbuf;
		rte_pktmbuf_free(mbuf);

		tx_info->mbuf = NULL;
		tx_ring->empty_tx_reqs[next_to_clean] = req_id;

		total_tx_descs += tx_info->tx_descs;

		/* Put back descriptor to the ring for reuse */
		next_to_clean = ENA_IDX_NEXT_MASKED(next_to_clean,
			tx_ring->size_mask);
	}

	if (likely(total_tx_descs > 0)) {
		/* acknowledge completion of sent packets */
		tx_ring->next_to_clean = next_to_clean;
		ena_com_comp_ack(tx_ring->ena_com_io_sq, total_tx_descs);
		ena_com_update_dev_comp_head(tx_ring->ena_com_io_cq);
	}
}

static uint16_t eth_ena_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts)
{
	struct ena_ring *tx_ring = (struct ena_ring *)(tx_queue);
	uint16_t sent_idx = 0;

	/* Check adapter state */
	if (unlikely(tx_ring->adapter->state != ENA_ADAPTER_STATE_RUNNING)) {
		PMD_DRV_LOG(ALERT,
			"Trying to xmit pkts while device is NOT running\n");
		return 0;
	}

	for (sent_idx = 0; sent_idx < nb_pkts; sent_idx++) {
		if (ena_xmit_mbuf(tx_ring, tx_pkts[sent_idx]))
			break;
		tx_ring->pkts_without_db = true;
		rte_prefetch0(tx_pkts[ENA_IDX_ADD_MASKED(sent_idx, 4,
			tx_ring->size_mask)]);
	}

	tx_ring->tx_stats.available_desc =
		ena_com_free_q_entries(tx_ring->ena_com_io_sq);

	/* If there are ready packets to be xmitted... */
	if (likely(tx_ring->pkts_without_db)) {
		/* ...let HW do its best :-) */
		ena_com_write_sq_doorbell(tx_ring->ena_com_io_sq);
		tx_ring->tx_stats.doorbells++;
		tx_ring->pkts_without_db = false;
	}

	ena_tx_cleanup(tx_ring);

	tx_ring->tx_stats.available_desc =
		ena_com_free_q_entries(tx_ring->ena_com_io_sq);
	tx_ring->tx_stats.tx_poll++;

	return sent_idx;
}

int ena_copy_eni_stats(struct ena_adapter *adapter)
{
	struct ena_admin_eni_stats admin_eni_stats;
	int rc;

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ena_com_get_eni_stats(&adapter->ena_dev, &admin_eni_stats);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (rc != 0) {
		if (rc == ENA_COM_UNSUPPORTED) {
			PMD_DRV_LOG(DEBUG,
				"Retrieving ENI metrics is not supported.\n");
		} else {
			PMD_DRV_LOG(WARNING,
				"Failed to get ENI metrics: %d\n", rc);
		}
		return rc;
	}

	rte_memcpy(&adapter->eni_stats, &admin_eni_stats,
		sizeof(struct ena_stats_eni));

	return 0;
}

/**
 * DPDK callback to retrieve names of extended device statistics
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] xstats_names
 *   Buffer to insert names into.
 * @param n
 *   Number of names.
 *
 * @return
 *   Number of xstats names.
 */
static int ena_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int n)
{
	unsigned int xstats_count = ena_xstats_calc_num(dev);
	unsigned int stat, i, count = 0;

	if (n < xstats_count || !xstats_names)
		return xstats_count;

	for (stat = 0; stat < ENA_STATS_ARRAY_GLOBAL; stat++, count++)
		strcpy(xstats_names[count].name,
			ena_stats_global_strings[stat].name);

	for (stat = 0; stat < ENA_STATS_ARRAY_ENI; stat++, count++)
		strcpy(xstats_names[count].name,
			ena_stats_eni_strings[stat].name);

	for (stat = 0; stat < ENA_STATS_ARRAY_RX; stat++)
		for (i = 0; i < dev->data->nb_rx_queues; i++, count++)
			snprintf(xstats_names[count].name,
				sizeof(xstats_names[count].name),
				"rx_q%d_%s", i,
				ena_stats_rx_strings[stat].name);

	for (stat = 0; stat < ENA_STATS_ARRAY_TX; stat++)
		for (i = 0; i < dev->data->nb_tx_queues; i++, count++)
			snprintf(xstats_names[count].name,
				sizeof(xstats_names[count].name),
				"tx_q%d_%s", i,
				ena_stats_tx_strings[stat].name);

	return xstats_count;
}

/**
 * DPDK callback to get extended device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats table output buffer.
 * @param n
 *   The size of the stats table.
 *
 * @return
 *   Number of xstats on success, negative on failure.
 */
static int ena_xstats_get(struct rte_eth_dev *dev,
			  struct rte_eth_xstat *xstats,
			  unsigned int n)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	unsigned int xstats_count = ena_xstats_calc_num(dev);
	unsigned int stat, i, count = 0;
	int stat_offset;
	void *stats_begin;

	if (n < xstats_count)
		return xstats_count;

	if (!xstats)
		return 0;

	for (stat = 0; stat < ENA_STATS_ARRAY_GLOBAL; stat++, count++) {
		stat_offset = ena_stats_global_strings[stat].stat_offset;
		stats_begin = &adapter->dev_stats;

		xstats[count].id = count;
		xstats[count].value = *((uint64_t *)
			((char *)stats_begin + stat_offset));
	}

	/* Even if the function below fails, we should copy previous (or initial
	 * values) to keep structure of rte_eth_xstat consistent.
	 */
	ena_copy_eni_stats(adapter);
	for (stat = 0; stat < ENA_STATS_ARRAY_ENI; stat++, count++) {
		stat_offset = ena_stats_eni_strings[stat].stat_offset;
		stats_begin = &adapter->eni_stats;

		xstats[count].id = count;
		xstats[count].value = *((uint64_t *)
		    ((char *)stats_begin + stat_offset));
	}

	for (stat = 0; stat < ENA_STATS_ARRAY_RX; stat++) {
		for (i = 0; i < dev->data->nb_rx_queues; i++, count++) {
			stat_offset = ena_stats_rx_strings[stat].stat_offset;
			stats_begin = &adapter->rx_ring[i].rx_stats;

			xstats[count].id = count;
			xstats[count].value = *((uint64_t *)
				((char *)stats_begin + stat_offset));
		}
	}

	for (stat = 0; stat < ENA_STATS_ARRAY_TX; stat++) {
		for (i = 0; i < dev->data->nb_tx_queues; i++, count++) {
			stat_offset = ena_stats_tx_strings[stat].stat_offset;
			stats_begin = &adapter->tx_ring[i].rx_stats;

			xstats[count].id = count;
			xstats[count].value = *((uint64_t *)
				((char *)stats_begin + stat_offset));
		}
	}

	return count;
}

static int ena_xstats_get_by_id(struct rte_eth_dev *dev,
				const uint64_t *ids,
				uint64_t *values,
				unsigned int n)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	uint64_t id;
	uint64_t rx_entries, tx_entries;
	unsigned int i;
	int qid;
	int valid = 0;
	bool was_eni_copied = false;

	for (i = 0; i < n; ++i) {
		id = ids[i];
		/* Check if id belongs to global statistics */
		if (id < ENA_STATS_ARRAY_GLOBAL) {
			values[i] = *((uint64_t *)&adapter->dev_stats + id);
			++valid;
			continue;
		}

		/* Check if id belongs to ENI statistics */
		id -= ENA_STATS_ARRAY_GLOBAL;
		if (id < ENA_STATS_ARRAY_ENI) {
			/* Avoid reading ENI stats multiple times in a single
			 * function call, as it requires communication with the
			 * admin queue.
			 */
			if (!was_eni_copied) {
				was_eni_copied = true;
				ena_copy_eni_stats(adapter);
			}
			values[i] = *((uint64_t *)&adapter->eni_stats + id);
			++valid;
			continue;
		}

		/* Check if id belongs to rx queue statistics */
		id -= ENA_STATS_ARRAY_ENI;
		rx_entries = ENA_STATS_ARRAY_RX * dev->data->nb_rx_queues;
		if (id < rx_entries) {
			qid = id % dev->data->nb_rx_queues;
			id /= dev->data->nb_rx_queues;
			values[i] = *((uint64_t *)
				&adapter->rx_ring[qid].rx_stats + id);
			++valid;
			continue;
		}
				/* Check if id belongs to rx queue statistics */
		id -= rx_entries;
		tx_entries = ENA_STATS_ARRAY_TX * dev->data->nb_tx_queues;
		if (id < tx_entries) {
			qid = id % dev->data->nb_tx_queues;
			id /= dev->data->nb_tx_queues;
			values[i] = *((uint64_t *)
				&adapter->tx_ring[qid].tx_stats + id);
			++valid;
			continue;
		}
	}

	return valid;
}

static int ena_process_bool_devarg(const char *key,
				   const char *value,
				   void *opaque)
{
	struct ena_adapter *adapter = opaque;
	bool bool_value;

	/* Parse the value. */
	if (strcmp(value, "1") == 0) {
		bool_value = true;
	} else if (strcmp(value, "0") == 0) {
		bool_value = false;
	} else {
		PMD_INIT_LOG(ERR,
			"Invalid value: '%s' for key '%s'. Accepted: '0' or '1'\n",
			value, key);
		return -EINVAL;
	}

	/* Now, assign it to the proper adapter field. */
	if (strcmp(key, ENA_DEVARG_LARGE_LLQ_HDR) == 0)
		adapter->use_large_llq_hdr = bool_value;

	return 0;
}

static int ena_parse_devargs(struct ena_adapter *adapter,
			     struct rte_devargs *devargs)
{
	static const char * const allowed_args[] = {
		ENA_DEVARG_LARGE_LLQ_HDR,
		NULL,
	};
	struct rte_kvargs *kvlist;
	int rc;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, allowed_args);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "Invalid device arguments: %s\n",
			devargs->args);
		return -EINVAL;
	}

	rc = rte_kvargs_process(kvlist, ENA_DEVARG_LARGE_LLQ_HDR,
		ena_process_bool_devarg, adapter);

	rte_kvargs_free(kvlist);

	return rc;
}

/*********************************************************************
 *  PMD configuration
 *********************************************************************/
static int eth_ena_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct ena_adapter), eth_ena_dev_init);
}

static int eth_ena_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_ena_dev_uninit);
}

static struct rte_pci_driver rte_ena_pmd = {
	.id_table = pci_id_ena_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_WC_ACTIVATE,
	.probe = eth_ena_pci_probe,
	.remove = eth_ena_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ena, rte_ena_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ena, pci_id_ena_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ena, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_ena, ENA_DEVARG_LARGE_LLQ_HDR "=<0|1>");
RTE_LOG_REGISTER(ena_logtype_init, pmd.net.ena.init, NOTICE);
RTE_LOG_REGISTER(ena_logtype_driver, pmd.net.ena.driver, NOTICE);
#ifdef RTE_LIBRTE_ENA_DEBUG_RX
RTE_LOG_REGISTER(ena_logtype_rx, pmd.net.ena.rx, NOTICE);
#endif
#ifdef RTE_LIBRTE_ENA_DEBUG_TX
RTE_LOG_REGISTER(ena_logtype_tx, pmd.net.ena.tx, NOTICE);
#endif
#ifdef RTE_LIBRTE_ENA_DEBUG_TX_FREE
RTE_LOG_REGISTER(ena_logtype_tx_free, pmd.net.ena.tx_free, NOTICE);
#endif
#ifdef RTE_LIBRTE_ENA_COM_DEBUG
RTE_LOG_REGISTER(ena_logtype_com, pmd.net.ena.com, NOTICE);
#endif

/******************************************************************************
 ******************************** AENQ Handlers *******************************
 *****************************************************************************/
static void ena_update_on_link_change(void *adapter_data,
				      struct ena_admin_aenq_entry *aenq_e)
{
	struct rte_eth_dev *eth_dev;
	struct ena_adapter *adapter;
	struct ena_admin_aenq_link_change_desc *aenq_link_desc;
	uint32_t status;

	adapter = adapter_data;
	aenq_link_desc = (struct ena_admin_aenq_link_change_desc *)aenq_e;
	eth_dev = adapter->rte_dev;

	status = get_ena_admin_aenq_link_change_desc_link_status(aenq_link_desc);
	adapter->link_status = status;

	ena_link_update(eth_dev, 0);
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

static void ena_notification(void *data,
			     struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = data;
	struct ena_admin_ena_hw_hints *hints;

	if (aenq_e->aenq_common_desc.group != ENA_ADMIN_NOTIFICATION)
		PMD_DRV_LOG(WARNING, "Invalid group(%x) expected %x\n",
			aenq_e->aenq_common_desc.group,
			ENA_ADMIN_NOTIFICATION);

	switch (aenq_e->aenq_common_desc.syndrome) {
	case ENA_ADMIN_UPDATE_HINTS:
		hints = (struct ena_admin_ena_hw_hints *)
			(&aenq_e->inline_data_w4);
		ena_update_hints(adapter, hints);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid aenq notification link state %d\n",
			aenq_e->aenq_common_desc.syndrome);
	}
}

static void ena_keep_alive(void *adapter_data,
			   __rte_unused struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = adapter_data;
	struct ena_admin_aenq_keep_alive_desc *desc;
	uint64_t rx_drops;
	uint64_t tx_drops;

	adapter->timestamp_wd = rte_get_timer_cycles();

	desc = (struct ena_admin_aenq_keep_alive_desc *)aenq_e;
	rx_drops = ((uint64_t)desc->rx_drops_high << 32) | desc->rx_drops_low;
	tx_drops = ((uint64_t)desc->tx_drops_high << 32) | desc->tx_drops_low;

	adapter->drv_stats->rx_drops = rx_drops;
	adapter->dev_stats.tx_drops = tx_drops;
}

/**
 * This handler will called for unknown event group or unimplemented handlers
 **/
static void unimplemented_aenq_handler(__rte_unused void *data,
				       __rte_unused struct ena_admin_aenq_entry *aenq_e)
{
	PMD_DRV_LOG(ERR, "Unknown event was received or event with "
			  "unimplemented handler\n");
}

static struct ena_aenq_handlers aenq_handlers = {
	.handlers = {
		[ENA_ADMIN_LINK_CHANGE] = ena_update_on_link_change,
		[ENA_ADMIN_NOTIFICATION] = ena_notification,
		[ENA_ADMIN_KEEP_ALIVE] = ena_keep_alive
	},
	.unimplemented_handler = unimplemented_aenq_handler
};
