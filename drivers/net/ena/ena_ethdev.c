/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#include <rte_string_fns.h>
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
#define DRV_MODULE_VER_MINOR	8
#define DRV_MODULE_VER_SUBMINOR	0

#define __MERGE_64B_H_L(h, l) (((uint64_t)h << 32) | l)

#define GET_L4_HDR_LEN(mbuf)					\
	((rte_pktmbuf_mtod_offset(mbuf,	struct rte_tcp_hdr *,	\
		mbuf->l3_len + mbuf->l2_len)->data_off) >> 4)

#define ETH_GSTRING_LEN	32

#define ARRAY_SIZE(x) RTE_DIM(x)

#define ENA_MIN_RING_DESC	128

/*
 * We should try to keep ENA_CLEANUP_BUF_THRESH lower than
 * RTE_MEMPOOL_CACHE_MAX_SIZE, so we can fit this in mempool local cache.
 */
#define ENA_CLEANUP_BUF_THRESH	256

#define ENA_PTYPE_HAS_HASH	(RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP)

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

#define ENA_STAT_METRICS_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, metrics)

#define ENA_STAT_GLOBAL_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, dev)

#define ENA_STAT_ENA_SRD_ENTRY(stat) \
	ENA_STAT_ENTRY(stat, srd)

/* Device arguments */
#define ENA_DEVARG_LARGE_LLQ_HDR "large_llq_hdr"
/* Timeout in seconds after which a single uncompleted Tx packet should be
 * considered as a missing.
 */
#define ENA_DEVARG_MISS_TXC_TO "miss_txc_to"
/*
 * Controls whether LLQ should be used (if available). Enabled by default.
 * NOTE: It's highly not recommended to disable the LLQ, as it may lead to a
 * huge performance degradation on 6th generation AWS instances.
 */
#define ENA_DEVARG_ENABLE_LLQ "enable_llq"

/*
 * Each rte_memzone should have unique name.
 * To satisfy it, count number of allocation and add it to name.
 */
rte_atomic64_t ena_alloc_cnt;

static const struct ena_stats ena_stats_global_strings[] = {
	ENA_STAT_GLOBAL_ENTRY(wd_expired),
	ENA_STAT_GLOBAL_ENTRY(dev_start),
	ENA_STAT_GLOBAL_ENTRY(dev_stop),
	ENA_STAT_GLOBAL_ENTRY(tx_drops),
	ENA_STAT_GLOBAL_ENTRY(rx_overruns),
};

/*
 * The legacy metrics (also known as eni stats) consisted of 5 stats, while the reworked
 * metrics (also known as customer metrics) support an additional stat.
 */
static struct ena_stats ena_stats_metrics_strings[] = {
	ENA_STAT_METRICS_ENTRY(bw_in_allowance_exceeded),
	ENA_STAT_METRICS_ENTRY(bw_out_allowance_exceeded),
	ENA_STAT_METRICS_ENTRY(pps_allowance_exceeded),
	ENA_STAT_METRICS_ENTRY(conntrack_allowance_exceeded),
	ENA_STAT_METRICS_ENTRY(linklocal_allowance_exceeded),
	ENA_STAT_METRICS_ENTRY(conntrack_allowance_available),
};

static const struct ena_stats ena_stats_srd_strings[] = {
	ENA_STAT_ENA_SRD_ENTRY(ena_srd_mode),
	ENA_STAT_ENA_SRD_ENTRY(ena_srd_tx_pkts),
	ENA_STAT_ENA_SRD_ENTRY(ena_srd_eligible_tx_pkts),
	ENA_STAT_ENA_SRD_ENTRY(ena_srd_rx_pkts),
	ENA_STAT_ENA_SRD_ENTRY(ena_srd_resource_utilization),
};

static const struct ena_stats ena_stats_tx_strings[] = {
	ENA_STAT_TX_ENTRY(cnt),
	ENA_STAT_TX_ENTRY(bytes),
	ENA_STAT_TX_ENTRY(prepare_ctx_err),
	ENA_STAT_TX_ENTRY(tx_poll),
	ENA_STAT_TX_ENTRY(doorbells),
	ENA_STAT_TX_ENTRY(bad_req_id),
	ENA_STAT_TX_ENTRY(available_desc),
	ENA_STAT_TX_ENTRY(missed_tx),
};

static const struct ena_stats ena_stats_rx_strings[] = {
	ENA_STAT_RX_ENTRY(cnt),
	ENA_STAT_RX_ENTRY(bytes),
	ENA_STAT_RX_ENTRY(refill_partial),
	ENA_STAT_RX_ENTRY(l3_csum_bad),
	ENA_STAT_RX_ENTRY(l4_csum_bad),
	ENA_STAT_RX_ENTRY(l4_csum_good),
	ENA_STAT_RX_ENTRY(mbuf_alloc_fail),
	ENA_STAT_RX_ENTRY(bad_desc_num),
	ENA_STAT_RX_ENTRY(bad_req_id),
};

#define ENA_STATS_ARRAY_GLOBAL	ARRAY_SIZE(ena_stats_global_strings)
#define ENA_STATS_ARRAY_METRICS	ARRAY_SIZE(ena_stats_metrics_strings)
#define ENA_STATS_ARRAY_METRICS_LEGACY	(ENA_STATS_ARRAY_METRICS - 1)
#define ENA_STATS_ARRAY_ENA_SRD	ARRAY_SIZE(ena_stats_srd_strings)
#define ENA_STATS_ARRAY_TX	ARRAY_SIZE(ena_stats_tx_strings)
#define ENA_STATS_ARRAY_RX	ARRAY_SIZE(ena_stats_rx_strings)

#define QUEUE_OFFLOADS (RTE_ETH_TX_OFFLOAD_TCP_CKSUM |\
			RTE_ETH_TX_OFFLOAD_UDP_CKSUM |\
			RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |\
			RTE_ETH_TX_OFFLOAD_TCP_TSO)
#define MBUF_OFFLOADS (RTE_MBUF_F_TX_L4_MASK |\
		       RTE_MBUF_F_TX_IP_CKSUM |\
		       RTE_MBUF_F_TX_TCP_SEG)

/** Vendor ID used by Amazon devices */
#define PCI_VENDOR_ID_AMAZON 0x1D0F
/** Amazon devices */
#define PCI_DEVICE_ID_ENA_VF		0xEC20
#define PCI_DEVICE_ID_ENA_VF_RSERV0	0xEC21

#define	ENA_TX_OFFLOAD_MASK	(RTE_MBUF_F_TX_L4_MASK |         \
	RTE_MBUF_F_TX_IPV6 |            \
	RTE_MBUF_F_TX_IPV4 |            \
	RTE_MBUF_F_TX_IP_CKSUM |        \
	RTE_MBUF_F_TX_TCP_SEG)

#define	ENA_TX_OFFLOAD_NOTSUP_MASK	\
	(RTE_MBUF_F_TX_OFFLOAD_MASK ^ ENA_TX_OFFLOAD_MASK)

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

static int ena_device_init(struct ena_adapter *adapter,
			   struct rte_pci_device *pdev,
			   struct ena_com_dev_get_features_ctx *get_feat_ctx);
static int ena_dev_configure(struct rte_eth_dev *dev);
static void ena_tx_map_mbuf(struct ena_ring *tx_ring,
	struct ena_tx_buffer *tx_info,
	struct rte_mbuf *mbuf,
	void **push_header,
	uint16_t *header_len);
static int ena_xmit_mbuf(struct ena_ring *tx_ring, struct rte_mbuf *mbuf);
static int ena_tx_cleanup(void *txp, uint32_t free_pkt_cnt);
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
static void ena_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
static void ena_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
static void ena_rx_queue_release_bufs(struct ena_ring *ring);
static void ena_tx_queue_release_bufs(struct ena_ring *ring);
static int ena_link_update(struct rte_eth_dev *dev,
			   int wait_to_complete);
static int ena_create_io_queue(struct rte_eth_dev *dev, struct ena_ring *ring);
static void ena_queue_stop(struct ena_ring *ring);
static void ena_queue_stop_all(struct rte_eth_dev *dev,
			      enum ena_ring_type ring_type);
static int ena_queue_start(struct rte_eth_dev *dev, struct ena_ring *ring);
static int ena_queue_start_all(struct rte_eth_dev *dev,
			       enum ena_ring_type ring_type);
static void ena_stats_restart(struct rte_eth_dev *dev);
static uint64_t ena_get_rx_port_offloads(struct ena_adapter *adapter);
static uint64_t ena_get_tx_port_offloads(struct ena_adapter *adapter);
static uint64_t ena_get_rx_queue_offloads(struct ena_adapter *adapter);
static uint64_t ena_get_tx_queue_offloads(struct ena_adapter *adapter);
static int ena_infos_get(struct rte_eth_dev *dev,
			 struct rte_eth_dev_info *dev_info);
static void ena_interrupt_handler_rte(void *cb_arg);
static void ena_timer_wd_callback(struct rte_timer *timer, void *arg);
static void ena_destroy_device(struct rte_eth_dev *eth_dev);
static int eth_ena_dev_init(struct rte_eth_dev *eth_dev);
static int ena_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int n);
static int ena_xstats_get_names_by_id(struct rte_eth_dev *dev,
				      const uint64_t *ids,
				      struct rte_eth_xstat_name *xstats_names,
				      unsigned int size);
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
static void ena_copy_customer_metrics(struct ena_adapter *adapter,
					uint64_t *buf,
					size_t buf_size);
static void ena_copy_ena_srd_info(struct ena_adapter *adapter,
				  struct ena_stats_srd *srd_info);
static int ena_setup_rx_intr(struct rte_eth_dev *dev);
static int ena_rx_queue_intr_enable(struct rte_eth_dev *dev,
				    uint16_t queue_id);
static int ena_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id);
static int ena_configure_aenq(struct ena_adapter *adapter);
static int ena_mp_primary_handle(const struct rte_mp_msg *mp_msg,
				 const void *peer);

static const struct eth_dev_ops ena_dev_ops = {
	.dev_configure          = ena_dev_configure,
	.dev_infos_get          = ena_infos_get,
	.rx_queue_setup         = ena_rx_queue_setup,
	.tx_queue_setup         = ena_tx_queue_setup,
	.dev_start              = ena_start,
	.dev_stop               = ena_stop,
	.link_update            = ena_link_update,
	.stats_get              = ena_stats_get,
	.xstats_get_names       = ena_xstats_get_names,
	.xstats_get_names_by_id = ena_xstats_get_names_by_id,
	.xstats_get             = ena_xstats_get,
	.xstats_get_by_id       = ena_xstats_get_by_id,
	.mtu_set                = ena_mtu_set,
	.rx_queue_release       = ena_rx_queue_release,
	.tx_queue_release       = ena_tx_queue_release,
	.dev_close              = ena_close,
	.dev_reset              = ena_dev_reset,
	.reta_update            = ena_rss_reta_update,
	.reta_query             = ena_rss_reta_query,
	.rx_queue_intr_enable   = ena_rx_queue_intr_enable,
	.rx_queue_intr_disable  = ena_rx_queue_intr_disable,
	.rss_hash_update        = ena_rss_hash_update,
	.rss_hash_conf_get      = ena_rss_hash_conf_get,
	.tx_done_cleanup        = ena_tx_cleanup,
};

/*********************************************************************
 *  Multi-Process communication bits
 *********************************************************************/
/* rte_mp IPC message name */
#define ENA_MP_NAME	"net_ena_mp"
/* Request timeout in seconds */
#define ENA_MP_REQ_TMO	5

/** Proxy request type */
enum ena_mp_req {
	ENA_MP_DEV_STATS_GET,
	ENA_MP_ENI_STATS_GET,
	ENA_MP_MTU_SET,
	ENA_MP_IND_TBL_GET,
	ENA_MP_IND_TBL_SET,
	ENA_MP_CUSTOMER_METRICS_GET,
	ENA_MP_SRD_STATS_GET,
};

/** Proxy message body. Shared between requests and responses. */
struct ena_mp_body {
	/* Message type */
	enum ena_mp_req type;
	int port_id;
	/* Processing result. Set in replies. 0 if message succeeded, negative
	 * error code otherwise.
	 */
	int result;
	union {
		int mtu; /* For ENA_MP_MTU_SET */
	} args;
};

/**
 * Initialize IPC message.
 *
 * @param[out] msg
 *   Pointer to the message to initialize.
 * @param[in] type
 *   Message type.
 * @param[in] port_id
 *   Port ID of target device.
 *
 */
static void
mp_msg_init(struct rte_mp_msg *msg, enum ena_mp_req type, int port_id)
{
	struct ena_mp_body *body = (struct ena_mp_body *)&msg->param;

	memset(msg, 0, sizeof(*msg));
	strlcpy(msg->name, ENA_MP_NAME, sizeof(msg->name));
	msg->len_param = sizeof(*body);
	body->type = type;
	body->port_id = port_id;
}

/*********************************************************************
 *  Multi-Process communication PMD API
 *********************************************************************/
/**
 * Define proxy request descriptor
 *
 * Used to define all structures and functions required for proxying a given
 * function to the primary process including the code to perform to prepare the
 * request and process the response.
 *
 * @param[in] f
 *   Name of the function to proxy
 * @param[in] t
 *   Message type to use
 * @param[in] prep
 *   Body of a function to prepare the request in form of a statement
 *   expression. It is passed all the original function arguments along with two
 *   extra ones:
 *   - struct ena_adapter *adapter - PMD data of the device calling the proxy.
 *   - struct ena_mp_body *req - body of a request to prepare.
 * @param[in] proc
 *   Body of a function to process the response in form of a statement
 *   expression. It is passed all the original function arguments along with two
 *   extra ones:
 *   - struct ena_adapter *adapter - PMD data of the device calling the proxy.
 *   - struct ena_mp_body *rsp - body of a response to process.
 * @param ...
 *   Proxied function's arguments
 *
 * @note Inside prep and proc any parameters which aren't used should be marked
 *       as such (with ENA_TOUCH or __rte_unused).
 */
#define ENA_PROXY_DESC(f, t, prep, proc, ...)			\
	static const enum ena_mp_req mp_type_ ## f =  t;	\
	static const char *mp_name_ ## f = #t;			\
	static void mp_prep_ ## f(struct ena_adapter *adapter,	\
				  struct ena_mp_body *req,	\
				  __VA_ARGS__)			\
	{							\
		prep;						\
	}							\
	static void mp_proc_ ## f(struct ena_adapter *adapter,	\
				  struct ena_mp_body *rsp,	\
				  __VA_ARGS__)			\
	{							\
		proc;						\
	}

/**
 * Proxy wrapper for calling primary functions in a secondary process.
 *
 * Depending on whether called in primary or secondary process, calls the
 * @p func directly or proxies the call to the primary process via rte_mp IPC.
 * This macro requires a proxy request descriptor to be defined for @p func
 * using ENA_PROXY_DESC() macro.
 *
 * @param[in/out] a
 *   Device PMD data. Used for sending the message and sharing message results
 *   between primary and secondary.
 * @param[in] f
 *   Function to proxy.
 * @param ...
 *   Arguments of @p func.
 *
 * @return
 *   - 0: Processing succeeded and response handler was called.
 *   - -EPERM: IPC is unavailable on this platform. This means only primary
 *             process may call the proxied function.
 *   - -EIO:   IPC returned error on request send. Inspect rte_errno detailed
 *             error code.
 *   - Negative error code from the proxied function.
 *
 * @note This mechanism is geared towards control-path tasks. Avoid calling it
 *       in fast-path unless unbound delays are allowed. This is due to the IPC
 *       mechanism itself (socket based).
 * @note Due to IPC parameter size limitations the proxy logic shares call
 *       results through the struct ena_adapter shared memory. This makes the
 *       proxy mechanism strictly single-threaded. Therefore be sure to make all
 *       calls to the same proxied function under the same lock.
 */
#define ENA_PROXY(a, f, ...)						\
({									\
	struct ena_adapter *_a = (a);					\
	struct timespec ts = { .tv_sec = ENA_MP_REQ_TMO };		\
	struct ena_mp_body *req, *rsp;					\
	struct rte_mp_reply mp_rep;					\
	struct rte_mp_msg mp_req;					\
	int ret;							\
									\
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {		\
		ret = f(__VA_ARGS__);					\
	} else {							\
		/* Prepare and send request */				\
		req = (struct ena_mp_body *)&mp_req.param;		\
		mp_msg_init(&mp_req, mp_type_ ## f, _a->edev_data->port_id); \
		mp_prep_ ## f(_a, req, ## __VA_ARGS__);			\
									\
		ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);	\
		if (likely(!ret)) {					\
			RTE_ASSERT(mp_rep.nb_received == 1);		\
			rsp = (struct ena_mp_body *)&mp_rep.msgs[0].param; \
			ret = rsp->result;				\
			if (ret == 0) {					\
				mp_proc_##f(_a, rsp, ## __VA_ARGS__);	\
			} else {					\
				PMD_DRV_LOG(ERR,			\
					    "%s returned error: %d\n",	\
					    mp_name_ ## f, rsp->result);\
			}						\
			free(mp_rep.msgs);				\
		} else if (rte_errno == ENOTSUP) {			\
			PMD_DRV_LOG(ERR,				\
				    "No IPC, can't proxy to primary\n");\
			ret = -rte_errno;				\
		} else {						\
			PMD_DRV_LOG(ERR, "Request %s failed: %s\n",	\
				    mp_name_ ## f,			\
				    rte_strerror(rte_errno));		\
			ret = -EIO;					\
		}							\
	}								\
	ret;								\
})

/*********************************************************************
 *  Multi-Process communication request descriptors
 *********************************************************************/

ENA_PROXY_DESC(ena_com_get_dev_basic_stats, ENA_MP_DEV_STATS_GET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(req);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(stats);
}),
({
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
	if (stats != &adapter->basic_stats)
		rte_memcpy(stats, &adapter->basic_stats, sizeof(*stats));
}),
	struct ena_com_dev *ena_dev, struct ena_admin_basic_stats *stats);

ENA_PROXY_DESC(ena_com_get_eni_stats, ENA_MP_ENI_STATS_GET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(req);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(stats);
}),
({
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
	if (stats != (struct ena_admin_eni_stats *)&adapter->metrics_stats)
		rte_memcpy(stats, &adapter->metrics_stats, sizeof(*stats));
}),
	struct ena_com_dev *ena_dev, struct ena_admin_eni_stats *stats);

ENA_PROXY_DESC(ena_com_set_dev_mtu, ENA_MP_MTU_SET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(ena_dev);
	req->args.mtu = mtu;
}),
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(mtu);
}),
	struct ena_com_dev *ena_dev, int mtu);

ENA_PROXY_DESC(ena_com_indirect_table_set, ENA_MP_IND_TBL_SET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(req);
	ENA_TOUCH(ena_dev);
}),
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
}),
	struct ena_com_dev *ena_dev);

ENA_PROXY_DESC(ena_com_indirect_table_get, ENA_MP_IND_TBL_GET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(req);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(ind_tbl);
}),
({
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
	if (ind_tbl != adapter->indirect_table)
		rte_memcpy(ind_tbl, adapter->indirect_table,
			   sizeof(adapter->indirect_table));
}),
	struct ena_com_dev *ena_dev, u32 *ind_tbl);

ENA_PROXY_DESC(ena_com_get_customer_metrics, ENA_MP_CUSTOMER_METRICS_GET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(req);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(buf);
	ENA_TOUCH(buf_size);
}),
({
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(buf_size);
	if (buf != (char *)&adapter->metrics_stats)
		rte_memcpy(buf, &adapter->metrics_stats, adapter->metrics_num * sizeof(uint64_t));
}),
	struct ena_com_dev *ena_dev, char *buf, size_t buf_size);

ENA_PROXY_DESC(ena_com_get_ena_srd_info, ENA_MP_SRD_STATS_GET,
({
	ENA_TOUCH(adapter);
	ENA_TOUCH(req);
	ENA_TOUCH(ena_dev);
	ENA_TOUCH(info);
}),
({
	ENA_TOUCH(rsp);
	ENA_TOUCH(ena_dev);
	if ((struct ena_stats_srd *)info != &adapter->srd_stats)
		rte_memcpy((struct ena_stats_srd *)info,
				&adapter->srd_stats,
				sizeof(struct ena_stats_srd));
}),
	struct ena_com_dev *ena_dev, struct ena_admin_ena_srd_info *info);

static inline void ena_trigger_reset(struct ena_adapter *adapter,
				     enum ena_regs_reset_reason_types reason)
{
	if (likely(!adapter->trigger_reset)) {
		adapter->reset_reason = reason;
		adapter->trigger_reset = true;
	}
}

static inline void ena_rx_mbuf_prepare(struct ena_ring *rx_ring,
				       struct rte_mbuf *mbuf,
				       struct ena_com_rx_ctx *ena_rx_ctx,
				       bool fill_hash)
{
	struct ena_stats_rx *rx_stats = &rx_ring->rx_stats;
	uint64_t ol_flags = 0;
	uint32_t packet_type = 0;

	if (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_TCP)
		packet_type |= RTE_PTYPE_L4_TCP;
	else if (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_UDP)
		packet_type |= RTE_PTYPE_L4_UDP;

	if (ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV4) {
		packet_type |= RTE_PTYPE_L3_IPV4;
		if (unlikely(ena_rx_ctx->l3_csum_err)) {
			++rx_stats->l3_csum_bad;
			ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		} else {
			ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		}
	} else if (ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV6) {
		packet_type |= RTE_PTYPE_L3_IPV6;
	}

	if (!ena_rx_ctx->l4_csum_checked || ena_rx_ctx->frag) {
		ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
	} else {
		if (unlikely(ena_rx_ctx->l4_csum_err)) {
			++rx_stats->l4_csum_bad;
			/*
			 * For the L4 Rx checksum offload the HW may indicate
			 * bad checksum although it's valid. Because of that,
			 * we're setting the UNKNOWN flag to let the app
			 * re-verify the checksum.
			 */
			ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
		} else {
			++rx_stats->l4_csum_good;
			ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		}
	}

	if (fill_hash &&
	    likely((packet_type & ENA_PTYPE_HAS_HASH) && !ena_rx_ctx->frag)) {
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
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
		if ((mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG) &&
		    (queue_offloads & RTE_ETH_TX_OFFLOAD_TCP_TSO)) {
			ena_tx_ctx->tso_enable = true;

			ena_meta->l4_hdr_len = GET_L4_HDR_LEN(mbuf);
		}

		/* check if L3 checksum is needed */
		if ((mbuf->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) &&
		    (queue_offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM))
			ena_tx_ctx->l3_csum_enable = true;

		if (mbuf->ol_flags & RTE_MBUF_F_TX_IPV6) {
			ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV6;
			/* For the IPv6 packets, DF always needs to be true. */
			ena_tx_ctx->df = 1;
		} else {
			ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV4;

			/* set don't fragment (DF) flag */
			if (mbuf->packet_type &
				(RTE_PTYPE_L4_NONFRAG
				 | RTE_PTYPE_INNER_L4_NONFRAG))
				ena_tx_ctx->df = 1;
		}

		/* check if L4 checksum is needed */
		if (((mbuf->ol_flags & RTE_MBUF_F_TX_L4_MASK) == RTE_MBUF_F_TX_TCP_CKSUM) &&
		    (queue_offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM)) {
			ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_TCP;
			ena_tx_ctx->l4_csum_enable = true;
		} else if (((mbuf->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
				RTE_MBUF_F_TX_UDP_CKSUM) &&
				(queue_offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM)) {
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
		PMD_TX_LOG(ERR, "tx_info doesn't have valid mbuf. queue %d:%d req_id %u\n",
			tx_ring->port_id, tx_ring->id, req_id);
	else
		PMD_TX_LOG(ERR, "Invalid req_id: %hu in queue %d:%d\n",
			req_id, tx_ring->port_id, tx_ring->id);

	/* Trigger device reset */
	++tx_ring->tx_stats.bad_req_id;
	ena_trigger_reset(tx_ring->adapter, ENA_REGS_RESET_INV_TX_REQ_ID);
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
		ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK |
		ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_MASK;

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
static unsigned int ena_xstats_calc_num(struct rte_eth_dev_data *data)
{
	struct ena_adapter *adapter = data->dev_private;

	return ENA_STATS_ARRAY_GLOBAL +
		adapter->metrics_num +
		ENA_STATS_ARRAY_ENA_SRD +
		(data->nb_tx_queues * ENA_STATS_ARRAY_TX) +
		(data->nb_rx_queues * ENA_STATS_ARRAY_RX);
}

static void ena_config_debug_area(struct ena_adapter *adapter)
{
	u32 debug_area_size;
	int rc, ss_count;

	ss_count = ena_xstats_calc_num(adapter->edev_data);

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
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
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
				     dev);

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

	/* Cannot release memory in secondary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(WARNING, "dev_reset not supported in secondary.\n");
		return -EPERM;
	}

	ena_destroy_device(dev);
	rc = eth_ena_dev_init(dev);
	if (rc)
		PMD_INIT_LOG(CRIT, "Cannot initialize device\n");

	return rc;
}

static void ena_rx_queue_release_all(struct rte_eth_dev *dev)
{
	int nb_queues = dev->data->nb_rx_queues;
	int i;

	for (i = 0; i < nb_queues; i++)
		ena_rx_queue_release(dev, i);
}

static void ena_tx_queue_release_all(struct rte_eth_dev *dev)
{
	int nb_queues = dev->data->nb_tx_queues;
	int i;

	for (i = 0; i < nb_queues; i++)
		ena_tx_queue_release(dev, i);
}

static void ena_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ena_ring *ring = dev->data->rx_queues[qid];

	/* Free ring resources */
	rte_free(ring->rx_buffer_info);
	ring->rx_buffer_info = NULL;

	rte_free(ring->rx_refill_buffer);
	ring->rx_refill_buffer = NULL;

	rte_free(ring->empty_rx_reqs);
	ring->empty_rx_reqs = NULL;

	ring->configured = 0;

	PMD_DRV_LOG(NOTICE, "Rx queue %d:%d released\n",
		ring->port_id, ring->id);
}

static void ena_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct ena_ring *ring = dev->data->tx_queues[qid];

	/* Free ring resources */
	rte_free(ring->push_buf_intermediate_buf);

	rte_free(ring->tx_buffer_info);

	rte_free(ring->empty_tx_reqs);

	ring->empty_tx_reqs = NULL;
	ring->tx_buffer_info = NULL;
	ring->push_buf_intermediate_buf = NULL;

	ring->configured = 0;

	PMD_DRV_LOG(NOTICE, "Tx queue %d:%d released\n",
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

	link->link_status = adapter->link_status ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	link->link_speed = RTE_ETH_SPEED_NUM_NONE;
	link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

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
					"Inconsistent state of Rx queues\n");
			} else {
				ena_assert_msg(
					dev->data->tx_queues[i] == &queues[i],
					"Inconsistent state of Tx queues\n");
			}

			rc = ena_queue_start(dev, &queues[i]);

			if (rc) {
				PMD_INIT_LOG(ERR,
					"Failed to start queue[%d] of type(%d)\n",
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
				"Forcing large headers and decreasing maximum Tx queue size to %d\n",
				max_tx_queue_size);
		} else {
			PMD_INIT_LOG(ERR,
				"Forcing large headers failed: LLQ is disabled or device does not support large headers\n");
		}
	}

	if (unlikely(max_rx_queue_size == 0 || max_tx_queue_size == 0)) {
		PMD_INIT_LOG(ERR, "Invalid queue size\n");
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

	memset(&ena_stats, 0, sizeof(ena_stats));

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ENA_PROXY(adapter, ena_com_get_dev_basic_stats, ena_dev,
		       &ena_stats);
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

	rc = ENA_PROXY(adapter, ena_com_set_dev_mtu, ena_dev, mtu);
	if (rc)
		PMD_DRV_LOG(ERR, "Could not set MTU: %d\n", mtu);
	else
		PMD_DRV_LOG(NOTICE, "MTU set to: %d\n", mtu);

	return rc;
}

static int ena_start(struct rte_eth_dev *dev)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	uint64_t ticks;
	int rc = 0;
	uint16_t i;

	/* Cannot allocate memory in secondary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(WARNING, "dev_start not supported in secondary.\n");
		return -EPERM;
	}

	rc = ena_setup_rx_intr(dev);
	if (rc)
		return rc;

	rc = ena_queue_start_all(dev, ENA_RING_TYPE_RX);
	if (rc)
		return rc;

	rc = ena_queue_start_all(dev, ENA_RING_TYPE_TX);
	if (rc)
		goto err_start_tx;

	if (adapter->edev_data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		rc = ena_rss_configure(adapter);
		if (rc)
			goto err_rss_init;
	}

	ena_stats_restart(dev);

	adapter->timestamp_wd = rte_get_timer_cycles();
	adapter->keep_alive_timeout = ENA_DEVICE_KALIVE_TIMEOUT;

	ticks = rte_get_timer_hz();
	rte_timer_reset(&adapter->timer_wd, ticks, PERIODICAL, rte_lcore_id(),
			ena_timer_wd_callback, dev);

	++adapter->dev_stats.dev_start;
	adapter->state = ENA_ADAPTER_STATE_RUNNING;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

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
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint16_t i;
	int rc;

	/* Cannot free memory in secondary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(WARNING, "dev_stop not supported in secondary.\n");
		return -EPERM;
	}

	rte_timer_stop_sync(&adapter->timer_wd);
	ena_queue_stop_all(dev, ENA_RING_TYPE_TX);
	ena_queue_stop_all(dev, ENA_RING_TYPE_RX);

	if (adapter->trigger_reset) {
		rc = ena_com_dev_reset(ena_dev, adapter->reset_reason);
		if (rc)
			PMD_DRV_LOG(ERR, "Device reset failed, rc: %d\n", rc);
	}

	rte_intr_disable(intr_handle);

	rte_intr_efd_disable(intr_handle);

	/* Cleanup vector list */
	rte_intr_vec_list_free(intr_handle);

	rte_intr_enable(intr_handle);

	++adapter->dev_stats.dev_stop;
	adapter->state = ENA_ADAPTER_STATE_STOPPED;
	dev->data->dev_started = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int ena_create_io_queue(struct rte_eth_dev *dev, struct ena_ring *ring)
{
	struct ena_adapter *adapter = ring->adapter;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct ena_com_create_io_ctx ctx =
		/* policy set to _HOST just to satisfy icc compiler */
		{ ENA_ADMIN_PLACEMENT_POLICY_HOST,
		  0, 0, 0, 0, 0 };
	uint16_t ena_qid;
	unsigned int i;
	int rc;

	ctx.msix_vector = -1;
	if (ring->type == ENA_RING_TYPE_TX) {
		ena_qid = ENA_IO_TXQ_IDX(ring->id);
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_TX;
		ctx.mem_queue_type = ena_dev->tx_mem_queue_type;
		for (i = 0; i < ring->ring_size; i++)
			ring->empty_tx_reqs[i] = i;
	} else {
		ena_qid = ENA_IO_RXQ_IDX(ring->id);
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_RX;
		if (rte_intr_dp_is_en(intr_handle))
			ctx.msix_vector =
				rte_intr_vec_list_index_get(intr_handle,
								   ring->id);

		for (i = 0; i < ring->ring_size; i++)
			ring->empty_rx_reqs[i] = i;
	}
	ctx.queue_size = ring->ring_size;
	ctx.qid = ena_qid;
	ctx.numa_node = ring->numa_socket_id;

	rc = ena_com_create_io_queue(ena_dev, &ctx);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Failed to create IO queue[%d] (qid:%d), rc: %d\n",
			ring->id, ena_qid, rc);
		return rc;
	}

	rc = ena_com_get_io_handlers(ena_dev, ena_qid,
				     &ring->ena_com_io_sq,
				     &ring->ena_com_io_cq);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Failed to get IO queue[%d] handlers, rc: %d\n",
			ring->id, rc);
		ena_com_destroy_io_queue(ena_dev, ena_qid);
		return rc;
	}

	if (ring->type == ENA_RING_TYPE_TX)
		ena_com_update_numa_node(ring->ena_com_io_cq, ctx.numa_node);

	/* Start with Rx interrupts being masked. */
	if (ring->type == ENA_RING_TYPE_RX && rte_intr_dp_is_en(intr_handle))
		ena_rx_queue_intr_disable(dev, ring->id);

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

static int ena_queue_start(struct rte_eth_dev *dev, struct ena_ring *ring)
{
	int rc, bufs_num;

	ena_assert_msg(ring->configured == 1,
		       "Trying to start unconfigured queue\n");

	rc = ena_create_io_queue(dev, ring);
	if (rc) {
		PMD_INIT_LOG(ERR, "Failed to create IO queue\n");
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
		PMD_INIT_LOG(ERR, "Failed to populate Rx ring\n");
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
	uint16_t dyn_thresh;

	txq = &adapter->tx_ring[queue_idx];

	if (txq->configured) {
		PMD_DRV_LOG(CRIT,
			"API violation. Queue[%d] is already configured\n",
			queue_idx);
		return ENA_COM_FAULT;
	}

	if (!rte_is_power_of_2(nb_desc)) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of Tx queue: %d is not a power of 2.\n",
			nb_desc);
		return -EINVAL;
	}

	if (nb_desc > adapter->max_tx_ring_size) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of Tx queue (max size: %d)\n",
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
	txq->last_cleanup_ticks = 0;

	txq->tx_buffer_info = rte_zmalloc_socket("txq->tx_buffer_info",
		sizeof(struct ena_tx_buffer) * txq->ring_size,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (!txq->tx_buffer_info) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate memory for Tx buffer info\n");
		return -ENOMEM;
	}

	txq->empty_tx_reqs = rte_zmalloc_socket("txq->empty_tx_reqs",
		sizeof(uint16_t) * txq->ring_size,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (!txq->empty_tx_reqs) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate memory for empty Tx requests\n");
		rte_free(txq->tx_buffer_info);
		return -ENOMEM;
	}

	txq->push_buf_intermediate_buf =
		rte_zmalloc_socket("txq->push_buf_intermediate_buf",
			txq->tx_max_header_size,
			RTE_CACHE_LINE_SIZE,
			socket_id);
	if (!txq->push_buf_intermediate_buf) {
		PMD_DRV_LOG(ERR, "Failed to alloc push buffer for LLQ\n");
		rte_free(txq->tx_buffer_info);
		rte_free(txq->empty_tx_reqs);
		return -ENOMEM;
	}

	for (i = 0; i < txq->ring_size; i++)
		txq->empty_tx_reqs[i] = i;

	txq->offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	/* Check if caller provided the Tx cleanup threshold value. */
	if (tx_conf->tx_free_thresh != 0) {
		txq->tx_free_thresh = tx_conf->tx_free_thresh;
	} else {
		dyn_thresh = txq->ring_size -
			txq->ring_size / ENA_REFILL_THRESH_DIVIDER;
		txq->tx_free_thresh = RTE_MAX(dyn_thresh,
			txq->ring_size - ENA_REFILL_THRESH_PACKET);
	}

	txq->missing_tx_completion_threshold =
		RTE_MIN(txq->ring_size / 2, ENA_DEFAULT_MISSING_COMP);

	/* Store pointer to this queue in upper layer */
	txq->configured = 1;
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

static int ena_rx_queue_setup(struct rte_eth_dev *dev,
			      uint16_t queue_idx,
			      uint16_t nb_desc,
			      unsigned int socket_id,
			      const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mp)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_ring *rxq = NULL;
	size_t buffer_size;
	int i;
	uint16_t dyn_thresh;

	rxq = &adapter->rx_ring[queue_idx];
	if (rxq->configured) {
		PMD_DRV_LOG(CRIT,
			"API violation. Queue[%d] is already configured\n",
			queue_idx);
		return ENA_COM_FAULT;
	}

	if (!rte_is_power_of_2(nb_desc)) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of Rx queue: %d is not a power of 2.\n",
			nb_desc);
		return -EINVAL;
	}

	if (nb_desc > adapter->max_rx_ring_size) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of Rx queue (max size: %d)\n",
			adapter->max_rx_ring_size);
		return -EINVAL;
	}

	/* ENA isn't supporting buffers smaller than 1400 bytes */
	buffer_size = rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM;
	if (buffer_size < ENA_RX_BUF_MIN_SIZE) {
		PMD_DRV_LOG(ERR,
			"Unsupported size of Rx buffer: %zu (min size: %d)\n",
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

	rxq->rx_buffer_info = rte_zmalloc_socket("rxq->buffer_info",
		sizeof(struct ena_rx_buffer) * nb_desc,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (!rxq->rx_buffer_info) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate memory for Rx buffer info\n");
		return -ENOMEM;
	}

	rxq->rx_refill_buffer = rte_zmalloc_socket("rxq->rx_refill_buffer",
		sizeof(struct rte_mbuf *) * nb_desc,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (!rxq->rx_refill_buffer) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate memory for Rx refill buffer\n");
		rte_free(rxq->rx_buffer_info);
		rxq->rx_buffer_info = NULL;
		return -ENOMEM;
	}

	rxq->empty_rx_reqs = rte_zmalloc_socket("rxq->empty_rx_reqs",
		sizeof(uint16_t) * nb_desc,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (!rxq->empty_rx_reqs) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate memory for empty Rx requests\n");
		rte_free(rxq->rx_buffer_info);
		rxq->rx_buffer_info = NULL;
		rte_free(rxq->rx_refill_buffer);
		rxq->rx_refill_buffer = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < nb_desc; i++)
		rxq->empty_rx_reqs[i] = i;

	rxq->offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	if (rx_conf->rx_free_thresh != 0) {
		rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	} else {
		dyn_thresh = rxq->ring_size / ENA_REFILL_THRESH_DIVIDER;
		rxq->rx_free_thresh = RTE_MIN(dyn_thresh,
			(uint16_t)(ENA_REFILL_THRESH_PACKET));
	}

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
		PMD_RX_LOG(WARNING, "Failed adding Rx desc\n");

	return rc;
}

static int ena_populate_rx_queue(struct ena_ring *rxq, unsigned int count)
{
	unsigned int i;
	int rc;
	uint16_t next_to_use = rxq->next_to_use;
	uint16_t req_id;
#ifdef RTE_ETHDEV_DEBUG_RX
	uint16_t in_use;
#endif
	struct rte_mbuf **mbufs = rxq->rx_refill_buffer;

	if (unlikely(!count))
		return 0;

#ifdef RTE_ETHDEV_DEBUG_RX
	in_use = rxq->ring_size - 1 -
		ena_com_free_q_entries(rxq->ena_com_io_sq);
	if (unlikely((in_use + count) >= rxq->ring_size))
		PMD_RX_LOG(ERR, "Bad Rx ring state\n");
#endif

	/* get resources for incoming packets */
	rc = rte_pktmbuf_alloc_bulk(rxq->mb_pool, mbufs, count);
	if (unlikely(rc < 0)) {
		rte_atomic64_inc(&rxq->adapter->drv_stats->rx_nombuf);
		++rxq->rx_stats.mbuf_alloc_fail;
		PMD_RX_LOG(DEBUG, "There are not enough free buffers\n");
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
		PMD_RX_LOG(WARNING,
			"Refilled Rx queue[%d] with only %d/%d buffers\n",
			rxq->id, i, count);
		rte_pktmbuf_free_bulk(&mbufs[i], count - i);
		++rxq->rx_stats.refill_partial;
	}

	/* When we submitted free resources to device... */
	if (likely(i > 0)) {
		/* ...let HW know that it can fill buffers with data. */
		ena_com_write_sq_doorbell(rxq->ena_com_io_sq);

		rxq->next_to_use = next_to_use;
	}

	return i;
}

static size_t ena_get_metrics_entries(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	size_t metrics_num = 0;

	if (ena_com_get_cap(ena_dev, ENA_ADMIN_CUSTOMER_METRICS))
		metrics_num = ENA_STATS_ARRAY_METRICS;
	else if (ena_com_get_cap(ena_dev, ENA_ADMIN_ENI_STATS))
		metrics_num = ENA_STATS_ARRAY_METRICS_LEGACY;
	PMD_DRV_LOG(NOTICE, "0x%x customer metrics are supported\n", (unsigned int)metrics_num);
	if (metrics_num > ENA_MAX_CUSTOMER_METRICS) {
		PMD_DRV_LOG(NOTICE, "Not enough space for the requested customer metrics\n");
		metrics_num = ENA_MAX_CUSTOMER_METRICS;
	}
	return metrics_num;
}

static int ena_device_init(struct ena_adapter *adapter,
			   struct rte_pci_device *pdev,
			   struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	uint32_t aenq_groups;
	int rc;
	bool readless_supported;

	/* Initialize mmio registers */
	rc = ena_com_mmio_reg_read_request_init(ena_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Failed to init MMIO read less\n");
		return rc;
	}

	/* The PCIe configuration space revision id indicate if mmio reg
	 * read is disabled.
	 */
	readless_supported = !(pdev->id.class_id & ENA_MMIO_DISABLE_REG_READ);
	ena_com_set_mmio_read_mode(ena_dev, readless_supported);

	/* reset device */
	rc = ena_com_dev_reset(ena_dev, ENA_REGS_RESET_NORMAL);
	if (rc) {
		PMD_DRV_LOG(ERR, "Cannot reset device\n");
		goto err_mmio_read_less;
	}

	/* check FW version */
	rc = ena_com_validate_version(ena_dev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Device version is too low\n");
		goto err_mmio_read_less;
	}

	ena_dev->dma_addr_bits = ena_com_get_dma_width(ena_dev);

	/* ENA device administration layer init */
	rc = ena_com_admin_init(ena_dev, &aenq_handlers);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Cannot initialize ENA admin queue\n");
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
			"Cannot get attribute for ENA device, rc: %d\n", rc);
		goto err_admin_init;
	}

	aenq_groups = BIT(ENA_ADMIN_LINK_CHANGE) |
		      BIT(ENA_ADMIN_NOTIFICATION) |
		      BIT(ENA_ADMIN_KEEP_ALIVE) |
		      BIT(ENA_ADMIN_FATAL_ERROR) |
		      BIT(ENA_ADMIN_WARNING);

	aenq_groups &= get_feat_ctx->aenq.supported_groups;

	adapter->all_aenq_groups = aenq_groups;
	/* The actual supported number of metrics is negotiated with the device at runtime */
	adapter->metrics_num = ena_get_metrics_entries(adapter);

	return 0;

err_admin_init:
	ena_com_admin_destroy(ena_dev);

err_mmio_read_less:
	ena_com_mmio_reg_read_request_destroy(ena_dev);

	return rc;
}

static void ena_interrupt_handler_rte(void *cb_arg)
{
	struct rte_eth_dev *dev = cb_arg;
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_com_dev *ena_dev = &adapter->ena_dev;

	ena_com_admin_q_comp_intr_handler(ena_dev);
	if (likely(adapter->state != ENA_ADAPTER_STATE_CLOSED))
		ena_com_aenq_intr_handler(ena_dev, dev);
}

static void check_for_missing_keep_alive(struct ena_adapter *adapter)
{
	if (!(adapter->active_aenq_groups & BIT(ENA_ADMIN_KEEP_ALIVE)))
		return;

	if (adapter->keep_alive_timeout == ENA_HW_HINTS_NO_TIMEOUT)
		return;

	if (unlikely((rte_get_timer_cycles() - adapter->timestamp_wd) >=
	    adapter->keep_alive_timeout)) {
		PMD_DRV_LOG(ERR, "Keep alive timeout\n");
		ena_trigger_reset(adapter, ENA_REGS_RESET_KEEP_ALIVE_TO);
		++adapter->dev_stats.wd_expired;
	}
}

/* Check if admin queue is enabled */
static void check_for_admin_com_state(struct ena_adapter *adapter)
{
	if (unlikely(!ena_com_get_admin_running_state(&adapter->ena_dev))) {
		PMD_DRV_LOG(ERR, "ENA admin queue is not in running state\n");
		ena_trigger_reset(adapter, ENA_REGS_RESET_ADMIN_TO);
	}
}

static int check_for_tx_completion_in_queue(struct ena_adapter *adapter,
					    struct ena_ring *tx_ring)
{
	struct ena_tx_buffer *tx_buf;
	uint64_t timestamp;
	uint64_t completion_delay;
	uint32_t missed_tx = 0;
	unsigned int i;
	int rc = 0;

	for (i = 0; i < tx_ring->ring_size; ++i) {
		tx_buf = &tx_ring->tx_buffer_info[i];
		timestamp = tx_buf->timestamp;

		if (timestamp == 0)
			continue;

		completion_delay = rte_get_timer_cycles() - timestamp;
		if (completion_delay > adapter->missing_tx_completion_to) {
			if (unlikely(!tx_buf->print_once)) {
				PMD_TX_LOG(WARNING,
					"Found a Tx that wasn't completed on time, qid %d, index %d. "
					"Missing Tx outstanding for %" PRIu64 " msecs.\n",
					tx_ring->id, i,	completion_delay /
					rte_get_timer_hz() * 1000);
				tx_buf->print_once = true;
			}
			++missed_tx;
		}
	}

	if (unlikely(missed_tx > tx_ring->missing_tx_completion_threshold)) {
		PMD_DRV_LOG(ERR,
			"The number of lost Tx completions is above the threshold (%d > %d). "
			"Trigger the device reset.\n",
			missed_tx,
			tx_ring->missing_tx_completion_threshold);
		adapter->reset_reason = ENA_REGS_RESET_MISS_TX_CMPL;
		adapter->trigger_reset = true;
		rc = -EIO;
	}

	tx_ring->tx_stats.missed_tx += missed_tx;

	return rc;
}

static void check_for_tx_completions(struct ena_adapter *adapter)
{
	struct ena_ring *tx_ring;
	uint64_t tx_cleanup_delay;
	size_t qid;
	int budget;
	uint16_t nb_tx_queues = adapter->edev_data->nb_tx_queues;

	if (adapter->missing_tx_completion_to == ENA_HW_HINTS_NO_TIMEOUT)
		return;

	nb_tx_queues = adapter->edev_data->nb_tx_queues;
	budget = adapter->missing_tx_completion_budget;

	qid = adapter->last_tx_comp_qid;
	while (budget-- > 0) {
		tx_ring = &adapter->tx_ring[qid];

		/* Tx cleanup is called only by the burst function and can be
		 * called dynamically by the application. Also cleanup is
		 * limited by the threshold. To avoid false detection of the
		 * missing HW Tx completion, get the delay since last cleanup
		 * function was called.
		 */
		tx_cleanup_delay = rte_get_timer_cycles() -
			tx_ring->last_cleanup_ticks;
		if (tx_cleanup_delay < adapter->tx_cleanup_stall_delay)
			check_for_tx_completion_in_queue(adapter, tx_ring);
		qid = (qid + 1) % nb_tx_queues;
	}

	adapter->last_tx_comp_qid = qid;
}

static void ena_timer_wd_callback(__rte_unused struct rte_timer *timer,
				  void *arg)
{
	struct rte_eth_dev *dev = arg;
	struct ena_adapter *adapter = dev->data->dev_private;

	if (unlikely(adapter->trigger_reset))
		return;

	check_for_missing_keep_alive(adapter);
	check_for_admin_com_state(adapter);
	check_for_tx_completions(adapter);

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

	if (!adapter->enable_llq) {
		PMD_DRV_LOG(WARNING,
			"NOTE: LLQ has been disabled as per user's request. "
			"This may lead to a huge performance degradation!\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	llq_feature_mask = 1 << ENA_ADMIN_LLQ;
	if (!(ena_dev->supported_features & llq_feature_mask)) {
		PMD_DRV_LOG(INFO,
			"LLQ is not supported. Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	if (adapter->dev_mem_base == NULL) {
		PMD_DRV_LOG(ERR,
			"LLQ is advertised as supported, but device doesn't expose mem bar\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	rc = ena_com_config_dev_mode(ena_dev, llq, llq_default_configurations);
	if (unlikely(rc)) {
		PMD_INIT_LOG(WARNING,
			"Failed to config dev mode. Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return 0;
	}

	/* Nothing to config, exit */
	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST)
		return 0;

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
		PMD_DRV_LOG(ERR, "Number of IO queues cannot not be 0\n");
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

static int ena_init_once(void)
{
	static bool init_done;

	if (init_done)
		return 0;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* Init timer subsystem for the ENA timer service. */
		rte_timer_subsystem_init();
		/* Register handler for requests from secondary processes. */
		rte_mp_action_register(ENA_MP_NAME, ena_mp_primary_handle);
	}

	init_done = true;
	return 0;
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

	eth_dev->dev_ops = &ena_dev_ops;
	eth_dev->rx_pkt_burst = &eth_ena_recv_pkts;
	eth_dev->tx_pkt_burst = &eth_ena_xmit_pkts;
	eth_dev->tx_pkt_prepare = &eth_ena_prep_pkts;

	rc = ena_init_once();
	if (rc != 0)
		return rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	memset(adapter, 0, sizeof(struct ena_adapter));
	ena_dev = &adapter->ena_dev;

	adapter->edev_data = eth_dev->data;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_INIT_LOG(INFO, "Initializing " PCI_PRI_FMT "\n",
		     pci_dev->addr.domain,
		     pci_dev->addr.bus,
		     pci_dev->addr.devid,
		     pci_dev->addr.function);

	intr_handle = pci_dev->intr_handle;

	adapter->regs = pci_dev->mem_resource[ENA_REGS_BAR].addr;
	adapter->dev_mem_base = pci_dev->mem_resource[ENA_MEM_BAR].addr;

	if (!adapter->regs) {
		PMD_INIT_LOG(CRIT, "Failed to access registers BAR(%d)\n",
			     ENA_REGS_BAR);
		return -ENXIO;
	}

	ena_dev->reg_bar = adapter->regs;
	/* Pass device data as a pointer which can be passed to the IO functions
	 * by the ena_com (for example - the memory allocation).
	 */
	ena_dev->dmadev = eth_dev->data;

	adapter->id_number = adapters_found;

	snprintf(adapter->name, ENA_NAME_MAX_LEN, "ena_%d",
		 adapter->id_number);

	/* Assign default devargs values */
	adapter->missing_tx_completion_to = ENA_TX_TIMEOUT;
	adapter->enable_llq = true;
	adapter->use_large_llq_hdr = false;

	rc = ena_parse_devargs(adapter, pci_dev->device.devargs);
	if (rc != 0) {
		PMD_INIT_LOG(CRIT, "Failed to parse devargs\n");
		goto err;
	}
	rc = ena_com_allocate_customer_metrics_buffer(ena_dev);
	if (rc != 0) {
		PMD_INIT_LOG(CRIT, "Failed to allocate customer metrics buffer\n");
		goto err;
	}

	/* device specific initialization routine */
	rc = ena_device_init(adapter, pci_dev, &get_feat_ctx);
	if (rc) {
		PMD_INIT_LOG(CRIT, "Failed to init ENA device\n");
		goto err_metrics_delete;
	}

	/* Check if device supports LSC */
	if (!(adapter->all_aenq_groups & BIT(ENA_ADMIN_LINK_CHANGE)))
		adapter->edev_data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	set_default_llq_configurations(&llq_config, &get_feat_ctx.llq,
		adapter->use_large_llq_hdr);
	rc = ena_set_queues_placement_policy(adapter, ena_dev,
					     &get_feat_ctx.llq, &llq_config);
	if (unlikely(rc)) {
		PMD_INIT_LOG(CRIT, "Failed to set placement policy\n");
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

	rc = ena_com_rss_init(ena_dev, ENA_RX_RSS_TABLE_LOG_SIZE);
	if (unlikely(rc != 0)) {
		PMD_DRV_LOG(ERR, "Failed to initialize RSS in ENA device\n");
		goto err_delete_debug_area;
	}

	adapter->drv_stats = rte_zmalloc("adapter stats",
					 sizeof(*adapter->drv_stats),
					 RTE_CACHE_LINE_SIZE);
	if (!adapter->drv_stats) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate memory for adapter statistics\n");
		rc = -ENOMEM;
		goto err_rss_destroy;
	}

	rte_spinlock_init(&adapter->admin_lock);

	rte_intr_callback_register(intr_handle,
				   ena_interrupt_handler_rte,
				   eth_dev);
	rte_intr_enable(intr_handle);
	ena_com_set_admin_polling_mode(ena_dev, false);
	ena_com_admin_aenq_enable(ena_dev);

	rte_timer_init(&adapter->timer_wd);

	adapters_found++;
	adapter->state = ENA_ADAPTER_STATE_INIT;

	return 0;

err_rss_destroy:
	ena_com_rss_destroy(ena_dev);
err_delete_debug_area:
	ena_com_delete_debug_area(ena_dev);

err_device_destroy:
	ena_com_delete_host_info(ena_dev);
	ena_com_admin_destroy(ena_dev);
err_metrics_delete:
	ena_com_delete_customer_metrics_buffer(ena_dev);
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

	ena_com_rss_destroy(ena_dev);

	ena_com_delete_debug_area(ena_dev);
	ena_com_delete_host_info(ena_dev);

	ena_com_abort_admin_commands(ena_dev);
	ena_com_wait_for_abort_completion(ena_dev);
	ena_com_admin_destroy(ena_dev);
	ena_com_mmio_reg_read_request_destroy(ena_dev);
	ena_com_delete_customer_metrics_buffer(ena_dev);

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
	int rc;

	adapter->state = ENA_ADAPTER_STATE_CONFIG;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
	dev->data->dev_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	/* Scattered Rx cannot be turned off in the HW, so this capability must
	 * be forced.
	 */
	dev->data->scattered_rx = 1;

	adapter->last_tx_comp_qid = 0;

	adapter->missing_tx_completion_budget =
		RTE_MIN(ENA_MONITORED_TX_QUEUES, dev->data->nb_tx_queues);

	/* To avoid detection of the spurious Tx completion timeout due to
	 * application not calling the Tx cleanup function, set timeout for the
	 * Tx queue which should be half of the missing completion timeout for a
	 * safety. If there will be a lot of missing Tx completions in the
	 * queue, they will be detected sooner or later.
	 */
	adapter->tx_cleanup_stall_delay = adapter->missing_tx_completion_to / 2;

	rc = ena_configure_aenq(adapter);

	return rc;
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
		port_offloads |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;

	if (adapter->offloads.rx_offloads &
	    (ENA_L4_IPV4_CSUM | ENA_L4_IPV6_CSUM))
		port_offloads |=
			RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM;

	if (adapter->offloads.rx_offloads & ENA_RX_RSS_HASH)
		port_offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	port_offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;

	return port_offloads;
}

static uint64_t ena_get_tx_port_offloads(struct ena_adapter *adapter)
{
	uint64_t port_offloads = 0;

	if (adapter->offloads.tx_offloads & ENA_IPV4_TSO)
		port_offloads |= RTE_ETH_TX_OFFLOAD_TCP_TSO;

	if (adapter->offloads.tx_offloads & ENA_L3_IPV4_CSUM)
		port_offloads |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
	if (adapter->offloads.tx_offloads &
	    (ENA_L4_IPV4_CSUM_PARTIAL | ENA_L4_IPV4_CSUM |
	     ENA_L4_IPV6_CSUM | ENA_L4_IPV6_CSUM_PARTIAL))
		port_offloads |=
			RTE_ETH_TX_OFFLOAD_UDP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_CKSUM;

	port_offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	port_offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	return port_offloads;
}

static uint64_t ena_get_rx_queue_offloads(struct ena_adapter *adapter)
{
	RTE_SET_USED(adapter);

	return 0;
}

static uint64_t ena_get_tx_queue_offloads(struct ena_adapter *adapter)
{
	uint64_t queue_offloads = 0;
	RTE_SET_USED(adapter);

	queue_offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	return queue_offloads;
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
			RTE_ETH_LINK_SPEED_1G   |
			RTE_ETH_LINK_SPEED_2_5G |
			RTE_ETH_LINK_SPEED_5G   |
			RTE_ETH_LINK_SPEED_10G  |
			RTE_ETH_LINK_SPEED_25G  |
			RTE_ETH_LINK_SPEED_40G  |
			RTE_ETH_LINK_SPEED_50G  |
			RTE_ETH_LINK_SPEED_100G;

	/* Inform framework about available features */
	dev_info->rx_offload_capa = ena_get_rx_port_offloads(adapter);
	dev_info->tx_offload_capa = ena_get_tx_port_offloads(adapter);
	dev_info->rx_queue_offload_capa = ena_get_rx_queue_offloads(adapter);
	dev_info->tx_queue_offload_capa = ena_get_tx_queue_offloads(adapter);

	dev_info->flow_type_rss_offloads = ENA_ALL_RSS_HF;
	dev_info->hash_key_size = ENA_HASH_KEY_SIZE;

	dev_info->min_rx_bufsize = ENA_MIN_FRAME_LEN;
	dev_info->max_rx_pktlen  = adapter->max_mtu + RTE_ETHER_HDR_LEN +
		RTE_ETHER_CRC_LEN;
	dev_info->min_mtu = ENA_MIN_MTU;
	dev_info->max_mtu = adapter->max_mtu;
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

	dev_info->err_handle_mode = RTE_ETH_ERROR_HANDLE_MODE_PASSIVE;

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
	uint16_t next_to_clean = rx_ring->next_to_clean;
	uint16_t descs_in_use;
	struct rte_mbuf *mbuf;
	uint16_t completed;
	struct ena_com_rx_ctx ena_rx_ctx;
	int i, rc = 0;
	bool fill_hash;

#ifdef RTE_ETHDEV_DEBUG_RX
	/* Check adapter state */
	if (unlikely(rx_ring->adapter->state != ENA_ADAPTER_STATE_RUNNING)) {
		PMD_RX_LOG(ALERT,
			"Trying to receive pkts while device is NOT running\n");
		return 0;
	}
#endif

	fill_hash = rx_ring->offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH;

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
			PMD_RX_LOG(ERR,
				"Failed to get the packet from the device, rc: %d\n",
				rc);
			if (rc == ENA_COM_NO_SPACE) {
				++rx_ring->rx_stats.bad_desc_num;
				ena_trigger_reset(rx_ring->adapter,
					ENA_REGS_RESET_TOO_MANY_RX_DESCS);
			} else {
				++rx_ring->rx_stats.bad_req_id;
				ena_trigger_reset(rx_ring->adapter,
					ENA_REGS_RESET_INV_RX_REQ_ID);
			}
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
		ena_rx_mbuf_prepare(rx_ring, mbuf, &ena_rx_ctx, fill_hash);

		if (unlikely(mbuf->ol_flags &
				(RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD)))
			rte_atomic64_inc(&rx_ring->adapter->drv_stats->ierrors);

		rx_pkts[completed] = mbuf;
		rx_ring->rx_stats.bytes += mbuf->pkt_len;
	}

	rx_ring->rx_stats.cnt += completed;
	rx_ring->next_to_clean = next_to_clean;

	free_queue_entries = ena_com_free_q_entries(rx_ring->ena_com_io_sq);

	/* Burst refill to save doorbells, memory barriers, const interval */
	if (free_queue_entries >= rx_ring->rx_free_thresh) {
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

		l4_csum_flag = ol_flags & RTE_MBUF_F_TX_L4_MASK;
		/* SCTP checksum offload is not supported by the ENA. */
		if ((ol_flags & ENA_TX_OFFLOAD_NOTSUP_MASK) ||
		    l4_csum_flag == RTE_MBUF_F_TX_SCTP_CKSUM) {
			PMD_TX_LOG(DEBUG,
				"mbuf[%" PRIu32 "] has unsupported offloads flags set: 0x%" PRIu64 "\n",
				i, ol_flags);
			rte_errno = ENOTSUP;
			return i;
		}

		if (unlikely(m->nb_segs >= tx_ring->sgl_size &&
		    !(tx_ring->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV &&
		      m->nb_segs == tx_ring->sgl_size &&
		      m->data_len < tx_ring->tx_max_header_size))) {
			PMD_TX_LOG(DEBUG,
				"mbuf[%" PRIu32 "] has too many segments: %" PRIu16 "\n",
				i, m->nb_segs);
			rte_errno = EINVAL;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		/* Check if requested offload is also enabled for the queue */
		if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM &&
		     !(tx_ring->offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM)) ||
		    (l4_csum_flag == RTE_MBUF_F_TX_TCP_CKSUM &&
		     !(tx_ring->offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM)) ||
		    (l4_csum_flag == RTE_MBUF_F_TX_UDP_CKSUM &&
		     !(tx_ring->offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM))) {
			PMD_TX_LOG(DEBUG,
				"mbuf[%" PRIu32 "]: requested offloads: %" PRIu16 " are not enabled for the queue[%u]\n",
				i, m->nb_segs, tx_ring->id);
			rte_errno = EINVAL;
			return i;
		}

		/* The caller is obligated to set l2 and l3 len if any cksum
		 * offload is enabled.
		 */
		if (unlikely(ol_flags & (RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_L4_MASK) &&
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
		if (ol_flags & RTE_MBUF_F_TX_IPV4) {
			if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM &&
			    !(dev_offload_capa & ENA_L3_IPV4_CSUM)) {
				rte_errno = ENOTSUP;
				return i;
			}

			if (ol_flags & RTE_MBUF_F_TX_TCP_SEG &&
			    !(dev_offload_capa & ENA_IPV4_TSO)) {
				rte_errno = ENOTSUP;
				return i;
			}

			/* Check HW capabilities and if pseudo csum is needed
			 * for L4 offloads.
			 */
			if (l4_csum_flag != RTE_MBUF_F_TX_L4_NO_CKSUM &&
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
			} else if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
				/* In case we are supposed to TSO and have DF
				 * not set (DF=0) hardware must be provided with
				 * partial checksum.
				 */
				need_pseudo_csum = true;
			}
		} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
			/* There is no support for IPv6 TSO as for now. */
			if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
				rte_errno = ENOTSUP;
				return i;
			}

			/* Check HW capabilities and if pseudo csum is needed */
			if (l4_csum_flag != RTE_MBUF_F_TX_L4_NO_CKSUM &&
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

	/* Checking for space for 2 additional metadata descriptors due to
	 * possible header split and metadata descriptor
	 */
	if (!ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
					  mbuf->nb_segs + 2)) {
		PMD_DRV_LOG(DEBUG, "Not enough space in the tx queue\n");
		return ENA_COM_NO_MEM;
	}

	next_to_use = tx_ring->next_to_use;

	req_id = tx_ring->empty_tx_reqs[next_to_use];
	tx_info = &tx_ring->tx_buffer_info[req_id];
	tx_info->num_of_bufs = 0;
	RTE_ASSERT(tx_info->mbuf == NULL);

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
		PMD_TX_LOG(DEBUG,
			"LLQ Tx max burst size of queue %d achieved, writing doorbell to send burst\n",
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
		ena_trigger_reset(tx_ring->adapter,
			ENA_REGS_RESET_DRIVER_INVALID_STATE);
		return rc;
	}

	tx_info->tx_descs = nb_hw_desc;
	tx_info->timestamp = rte_get_timer_cycles();

	tx_ring->tx_stats.cnt++;
	tx_ring->tx_stats.bytes += mbuf->pkt_len;

	tx_ring->next_to_use = ENA_IDX_NEXT_MASKED(next_to_use,
		tx_ring->size_mask);

	return 0;
}

static int ena_tx_cleanup(void *txp, uint32_t free_pkt_cnt)
{
	struct rte_mbuf *pkts_to_clean[ENA_CLEANUP_BUF_THRESH];
	struct ena_ring *tx_ring = (struct ena_ring *)txp;
	size_t mbuf_cnt = 0;
	size_t pkt_cnt = 0;
	unsigned int total_tx_descs = 0;
	unsigned int total_tx_pkts = 0;
	uint16_t cleanup_budget;
	uint16_t next_to_clean = tx_ring->next_to_clean;
	bool fast_free = tx_ring->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	/*
	 * If free_pkt_cnt is equal to 0, it means that the user requested
	 * full cleanup, so attempt to release all Tx descriptors
	 * (ring_size - 1 -> size_mask)
	 */
	cleanup_budget = (free_pkt_cnt == 0) ? tx_ring->size_mask : free_pkt_cnt;

	while (likely(total_tx_pkts < cleanup_budget)) {
		struct rte_mbuf *mbuf;
		struct ena_tx_buffer *tx_info;
		uint16_t req_id;

		if (ena_com_tx_comp_req_id_get(tx_ring->ena_com_io_cq, &req_id) != 0)
			break;

		if (unlikely(validate_tx_req_id(tx_ring, req_id) != 0))
			break;

		/* Get Tx info & store how many descs were processed  */
		tx_info = &tx_ring->tx_buffer_info[req_id];
		tx_info->timestamp = 0;

		mbuf = tx_info->mbuf;
		if (fast_free) {
			pkts_to_clean[pkt_cnt++] = mbuf;
			mbuf_cnt += mbuf->nb_segs;
			if (mbuf_cnt >= ENA_CLEANUP_BUF_THRESH) {
				rte_pktmbuf_free_bulk(pkts_to_clean, pkt_cnt);
				mbuf_cnt = 0;
				pkt_cnt = 0;
			}
		} else {
			rte_pktmbuf_free(mbuf);
		}

		tx_info->mbuf = NULL;
		tx_ring->empty_tx_reqs[next_to_clean] = req_id;

		total_tx_descs += tx_info->tx_descs;
		total_tx_pkts++;

		/* Put back descriptor to the ring for reuse */
		next_to_clean = ENA_IDX_NEXT_MASKED(next_to_clean,
			tx_ring->size_mask);
	}

	if (likely(total_tx_descs > 0)) {
		/* acknowledge completion of sent packets */
		tx_ring->next_to_clean = next_to_clean;
		ena_com_comp_ack(tx_ring->ena_com_io_sq, total_tx_descs);
	}

	if (mbuf_cnt != 0)
		rte_pktmbuf_free_bulk(pkts_to_clean, pkt_cnt);

	/* Notify completion handler that full cleanup was performed */
	if (free_pkt_cnt == 0 || total_tx_pkts < cleanup_budget)
		tx_ring->last_cleanup_ticks = rte_get_timer_cycles();

	return total_tx_pkts;
}

static uint16_t eth_ena_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts)
{
	struct ena_ring *tx_ring = (struct ena_ring *)(tx_queue);
	int available_desc;
	uint16_t sent_idx = 0;

#ifdef RTE_ETHDEV_DEBUG_TX
	/* Check adapter state */
	if (unlikely(tx_ring->adapter->state != ENA_ADAPTER_STATE_RUNNING)) {
		PMD_TX_LOG(ALERT,
			"Trying to xmit pkts while device is NOT running\n");
		return 0;
	}
#endif

	available_desc = ena_com_free_q_entries(tx_ring->ena_com_io_sq);
	if (available_desc < tx_ring->tx_free_thresh)
		ena_tx_cleanup((void *)tx_ring, 0);

	for (sent_idx = 0; sent_idx < nb_pkts; sent_idx++) {
		if (ena_xmit_mbuf(tx_ring, tx_pkts[sent_idx]))
			break;
		tx_ring->pkts_without_db = true;
		rte_prefetch0(tx_pkts[ENA_IDX_ADD_MASKED(sent_idx, 4,
			tx_ring->size_mask)]);
	}

	/* If there are ready packets to be xmitted... */
	if (likely(tx_ring->pkts_without_db)) {
		/* ...let HW do its best :-) */
		ena_com_write_sq_doorbell(tx_ring->ena_com_io_sq);
		tx_ring->tx_stats.doorbells++;
		tx_ring->pkts_without_db = false;
	}

	tx_ring->tx_stats.available_desc =
		ena_com_free_q_entries(tx_ring->ena_com_io_sq);
	tx_ring->tx_stats.tx_poll++;

	return sent_idx;
}

static void ena_copy_customer_metrics(struct ena_adapter *adapter, uint64_t *buf,
					     size_t num_metrics)
{
	struct ena_com_dev *ena_dev = &adapter->ena_dev;
	int rc;

	if (ena_com_get_cap(ena_dev, ENA_ADMIN_CUSTOMER_METRICS)) {
		if (num_metrics != ENA_STATS_ARRAY_METRICS) {
			PMD_DRV_LOG(ERR, "Detected discrepancy in the number of customer metrics");
			return;
		}
		rte_spinlock_lock(&adapter->admin_lock);
		rc = ENA_PROXY(adapter,
					ena_com_get_customer_metrics,
					&adapter->ena_dev,
					(char *)buf,
					num_metrics * sizeof(uint64_t));
		rte_spinlock_unlock(&adapter->admin_lock);
		if (rc != 0) {
			PMD_DRV_LOG(WARNING, "Failed to get customer metrics, rc: %d\n", rc);
			return;
		}

	} else if (ena_com_get_cap(ena_dev, ENA_ADMIN_ENI_STATS)) {
		if (num_metrics != ENA_STATS_ARRAY_METRICS_LEGACY) {
			PMD_DRV_LOG(ERR, "Detected discrepancy in the number of legacy metrics");
			return;
		}

		rte_spinlock_lock(&adapter->admin_lock);
		rc = ENA_PROXY(adapter,
			       ena_com_get_eni_stats,
			       &adapter->ena_dev,
			       (struct ena_admin_eni_stats *)buf);
		rte_spinlock_unlock(&adapter->admin_lock);
		if (rc != 0) {
			PMD_DRV_LOG(WARNING,
				"Failed to get ENI metrics, rc: %d\n", rc);
			return;
		}
	}
}

static void ena_copy_ena_srd_info(struct ena_adapter *adapter,
		struct ena_stats_srd *srd_info)
{
	int rc;

	if (!ena_com_get_cap(&adapter->ena_dev, ENA_ADMIN_ENA_SRD_INFO))
		return;

	rte_spinlock_lock(&adapter->admin_lock);
	rc = ENA_PROXY(adapter,
		       ena_com_get_ena_srd_info,
		       &adapter->ena_dev,
		       (struct ena_admin_ena_srd_info *)srd_info);
	rte_spinlock_unlock(&adapter->admin_lock);
	if (rc != ENA_COM_OK && rc != ENA_COM_UNSUPPORTED) {
		PMD_DRV_LOG(WARNING,
				"Failed to get ENA express srd info, rc: %d\n", rc);
		return;
	}
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
	struct ena_adapter *adapter = dev->data->dev_private;
	unsigned int xstats_count = ena_xstats_calc_num(dev->data);
	unsigned int stat, i, count = 0;

	if (n < xstats_count || !xstats_names)
		return xstats_count;

	for (stat = 0; stat < ENA_STATS_ARRAY_GLOBAL; stat++, count++)
		strcpy(xstats_names[count].name,
			ena_stats_global_strings[stat].name);

	for (stat = 0; stat < adapter->metrics_num; stat++, count++)
		rte_strscpy(xstats_names[count].name,
			    ena_stats_metrics_strings[stat].name,
			    RTE_ETH_XSTATS_NAME_SIZE);
	for (stat = 0; stat < ENA_STATS_ARRAY_ENA_SRD; stat++, count++)
		rte_strscpy(xstats_names[count].name,
			    ena_stats_srd_strings[stat].name,
			    RTE_ETH_XSTATS_NAME_SIZE);

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
 * DPDK callback to retrieve names of extended device statistics for the given
 * ids.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] xstats_names
 *   Buffer to insert names into.
 * @param ids
 *   IDs array for which the names should be retrieved.
 * @param size
 *   Number of ids.
 *
 * @return
 *   Positive value: number of xstats names. Negative value: error code.
 */
static int ena_xstats_get_names_by_id(struct rte_eth_dev *dev,
				      const uint64_t *ids,
				      struct rte_eth_xstat_name *xstats_names,
				      unsigned int size)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	uint64_t xstats_count = ena_xstats_calc_num(dev->data);
	uint64_t id, qid;
	unsigned int i;

	if (xstats_names == NULL)
		return xstats_count;

	for (i = 0; i < size; ++i) {
		id = ids[i];
		if (id > xstats_count) {
			PMD_DRV_LOG(ERR,
				"ID value out of range: id=%" PRIu64 ", xstats_num=%" PRIu64 "\n",
				 id, xstats_count);
			return -EINVAL;
		}

		if (id < ENA_STATS_ARRAY_GLOBAL) {
			strcpy(xstats_names[i].name,
			       ena_stats_global_strings[id].name);
			continue;
		}

		id -= ENA_STATS_ARRAY_GLOBAL;
		if (id < adapter->metrics_num) {
			rte_strscpy(xstats_names[i].name,
				    ena_stats_metrics_strings[id].name,
				    RTE_ETH_XSTATS_NAME_SIZE);
			continue;
		}

		id -= adapter->metrics_num;

		if (id < ENA_STATS_ARRAY_ENA_SRD) {
			rte_strscpy(xstats_names[i].name,
				    ena_stats_srd_strings[id].name,
				    RTE_ETH_XSTATS_NAME_SIZE);
			continue;
		}
		id -= ENA_STATS_ARRAY_ENA_SRD;

		if (id < ENA_STATS_ARRAY_RX) {
			qid = id / dev->data->nb_rx_queues;
			id %= dev->data->nb_rx_queues;
			snprintf(xstats_names[i].name,
				 sizeof(xstats_names[i].name),
				 "rx_q%" PRIu64 "d_%s",
				 qid, ena_stats_rx_strings[id].name);
			continue;
		}

		id -= ENA_STATS_ARRAY_RX;
		/* Although this condition is not needed, it was added for
		 * compatibility if new xstat structure would be ever added.
		 */
		if (id < ENA_STATS_ARRAY_TX) {
			qid = id / dev->data->nb_tx_queues;
			id %= dev->data->nb_tx_queues;
			snprintf(xstats_names[i].name,
				 sizeof(xstats_names[i].name),
				 "tx_q%" PRIu64 "_%s",
				 qid, ena_stats_tx_strings[id].name);
			continue;
		}
	}

	return i;
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
	unsigned int xstats_count = ena_xstats_calc_num(dev->data);
	unsigned int stat, i, count = 0;
	int stat_offset;
	void *stats_begin;
	uint64_t metrics_stats[ENA_MAX_CUSTOMER_METRICS];
	struct ena_stats_srd srd_info = {0};

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

	ena_copy_customer_metrics(adapter, metrics_stats, adapter->metrics_num);
	stats_begin = metrics_stats;
	for (stat = 0; stat < adapter->metrics_num; stat++, count++) {
		stat_offset = ena_stats_metrics_strings[stat].stat_offset;

		xstats[count].id = count;
		xstats[count].value = *((uint64_t *)
		    ((char *)stats_begin + stat_offset));
	}

	ena_copy_ena_srd_info(adapter, &srd_info);
	stats_begin = &srd_info;
	for (stat = 0; stat < ENA_STATS_ARRAY_ENA_SRD; stat++, count++) {
		stat_offset = ena_stats_srd_strings[stat].stat_offset;
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
	bool were_metrics_copied = false;
	bool was_srd_info_copied = false;
	uint64_t metrics_stats[ENA_MAX_CUSTOMER_METRICS];
	struct ena_stats_srd srd_info = {0};

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
		if (id < adapter->metrics_num) {
			/* Avoid reading metrics multiple times in a single
			 * function call, as it requires communication with the
			 * admin queue.
			 */
			if (!were_metrics_copied) {
				were_metrics_copied = true;
				ena_copy_customer_metrics(adapter,
						metrics_stats,
						adapter->metrics_num);
			}

			values[i] = *((uint64_t *)&metrics_stats + id);
			++valid;
			continue;
		}

		/* Check if id belongs to SRD info statistics */
		id -= adapter->metrics_num;

		if (id < ENA_STATS_ARRAY_ENA_SRD) {
			/*
			 * Avoid reading srd info multiple times in a single
			 * function call, as it requires communication with the
			 * admin queue.
			 */
			if (!was_srd_info_copied) {
				was_srd_info_copied = true;
				ena_copy_ena_srd_info(adapter, &srd_info);
			}
			values[i] = *((uint64_t *)&adapter->srd_stats + id);
			++valid;
			continue;
		}

		/* Check if id belongs to rx queue statistics */
		id -= ENA_STATS_ARRAY_ENA_SRD;

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

static int ena_process_uint_devarg(const char *key,
				  const char *value,
				  void *opaque)
{
	struct ena_adapter *adapter = opaque;
	char *str_end;
	uint64_t uint_value;

	uint_value = strtoull(value, &str_end, 10);
	if (value == str_end) {
		PMD_INIT_LOG(ERR,
			"Invalid value for key '%s'. Only uint values are accepted.\n",
			key);
		return -EINVAL;
	}

	if (strcmp(key, ENA_DEVARG_MISS_TXC_TO) == 0) {
		if (uint_value > ENA_MAX_TX_TIMEOUT_SECONDS) {
			PMD_INIT_LOG(ERR,
				"Tx timeout too high: %" PRIu64 " sec. Maximum allowed: %d sec.\n",
				uint_value, ENA_MAX_TX_TIMEOUT_SECONDS);
			return -EINVAL;
		} else if (uint_value == 0) {
			PMD_INIT_LOG(INFO,
				"Check for missing Tx completions has been disabled.\n");
			adapter->missing_tx_completion_to =
				ENA_HW_HINTS_NO_TIMEOUT;
		} else {
			PMD_INIT_LOG(INFO,
				"Tx packet completion timeout set to %" PRIu64 " seconds.\n",
				uint_value);
			adapter->missing_tx_completion_to =
				uint_value * rte_get_timer_hz();
		}
	}

	return 0;
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
	else if (strcmp(key, ENA_DEVARG_ENABLE_LLQ) == 0)
		adapter->enable_llq = bool_value;

	return 0;
}

static int ena_parse_devargs(struct ena_adapter *adapter,
			     struct rte_devargs *devargs)
{
	static const char * const allowed_args[] = {
		ENA_DEVARG_LARGE_LLQ_HDR,
		ENA_DEVARG_MISS_TXC_TO,
		ENA_DEVARG_ENABLE_LLQ,
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
	if (rc != 0)
		goto exit;
	rc = rte_kvargs_process(kvlist, ENA_DEVARG_MISS_TXC_TO,
		ena_process_uint_devarg, adapter);
	if (rc != 0)
		goto exit;
	rc = rte_kvargs_process(kvlist, ENA_DEVARG_ENABLE_LLQ,
		ena_process_bool_devarg, adapter);

exit:
	rte_kvargs_free(kvlist);

	return rc;
}

static int ena_setup_rx_intr(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int rc;
	uint16_t vectors_nb, i;
	bool rx_intr_requested = dev->data->dev_conf.intr_conf.rxq;

	if (!rx_intr_requested)
		return 0;

	if (!rte_intr_cap_multiple(intr_handle)) {
		PMD_DRV_LOG(ERR,
			"Rx interrupt requested, but it isn't supported by the PCI driver\n");
		return -ENOTSUP;
	}

	/* Disable interrupt mapping before the configuration starts. */
	rte_intr_disable(intr_handle);

	/* Verify if there are enough vectors available. */
	vectors_nb = dev->data->nb_rx_queues;
	if (vectors_nb > RTE_MAX_RXTX_INTR_VEC_ID) {
		PMD_DRV_LOG(ERR,
			"Too many Rx interrupts requested, maximum number: %d\n",
			RTE_MAX_RXTX_INTR_VEC_ID);
		rc = -ENOTSUP;
		goto enable_intr;
	}

	/* Allocate the vector list */
	if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
					   dev->data->nb_rx_queues)) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate interrupt vector for %d queues\n",
			dev->data->nb_rx_queues);
		rc = -ENOMEM;
		goto enable_intr;
	}

	rc = rte_intr_efd_enable(intr_handle, vectors_nb);
	if (rc != 0)
		goto free_intr_vec;

	if (!rte_intr_allow_others(intr_handle)) {
		PMD_DRV_LOG(ERR,
			"Not enough interrupts available to use both ENA Admin and Rx interrupts\n");
		goto disable_intr_efd;
	}

	for (i = 0; i < vectors_nb; ++i)
		if (rte_intr_vec_list_index_set(intr_handle, i,
					   RTE_INTR_VEC_RXTX_OFFSET + i))
			goto disable_intr_efd;

	rte_intr_enable(intr_handle);
	return 0;

disable_intr_efd:
	rte_intr_efd_disable(intr_handle);
free_intr_vec:
	rte_intr_vec_list_free(intr_handle);
enable_intr:
	rte_intr_enable(intr_handle);
	return rc;
}

static void ena_rx_queue_intr_set(struct rte_eth_dev *dev,
				 uint16_t queue_id,
				 bool unmask)
{
	struct ena_adapter *adapter = dev->data->dev_private;
	struct ena_ring *rxq = &adapter->rx_ring[queue_id];
	struct ena_eth_io_intr_reg intr_reg;

	ena_com_update_intr_reg(&intr_reg, 0, 0, unmask, 1);
	ena_com_unmask_intr(rxq->ena_com_io_cq, &intr_reg);
}

static int ena_rx_queue_intr_enable(struct rte_eth_dev *dev,
				    uint16_t queue_id)
{
	ena_rx_queue_intr_set(dev, queue_id, true);

	return 0;
}

static int ena_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id)
{
	ena_rx_queue_intr_set(dev, queue_id, false);

	return 0;
}

static int ena_configure_aenq(struct ena_adapter *adapter)
{
	uint32_t aenq_groups = adapter->all_aenq_groups;
	int rc;

	/* All_aenq_groups holds all AENQ functions supported by the device and
	 * the HW, so at first we need to be sure the LSC request is valid.
	 */
	if (adapter->edev_data->dev_conf.intr_conf.lsc != 0) {
		if (!(aenq_groups & BIT(ENA_ADMIN_LINK_CHANGE))) {
			PMD_DRV_LOG(ERR,
				"LSC requested, but it's not supported by the AENQ\n");
			return -EINVAL;
		}
	} else {
		/* If LSC wasn't enabled by the app, let's enable all supported
		 * AENQ procedures except the LSC.
		 */
		aenq_groups &= ~BIT(ENA_ADMIN_LINK_CHANGE);
	}

	rc = ena_com_set_aenq_config(&adapter->ena_dev, aenq_groups);
	if (rc != 0) {
		PMD_DRV_LOG(ERR, "Cannot configure AENQ groups, rc=%d\n", rc);
		return rc;
	}

	adapter->active_aenq_groups = aenq_groups;

	return 0;
}

int ena_mp_indirect_table_set(struct ena_adapter *adapter)
{
	return ENA_PROXY(adapter, ena_com_indirect_table_set, &adapter->ena_dev);
}

int ena_mp_indirect_table_get(struct ena_adapter *adapter,
			      uint32_t *indirect_table)
{
	return ENA_PROXY(adapter, ena_com_indirect_table_get, &adapter->ena_dev,
		indirect_table);
}

/*********************************************************************
 *  ena_plat_dpdk.h functions implementations
 *********************************************************************/

const struct rte_memzone *
ena_mem_alloc_coherent(struct rte_eth_dev_data *data, size_t size,
		       int socket_id, unsigned int alignment, void **virt_addr,
		       dma_addr_t *phys_addr)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	struct ena_adapter *adapter = data->dev_private;
	const struct rte_memzone *memzone;
	int rc;

	rc = snprintf(z_name, RTE_MEMZONE_NAMESIZE, "ena_p%d_mz%" PRIu64 "",
		data->port_id, adapter->memzone_cnt);
	if (rc >= RTE_MEMZONE_NAMESIZE) {
		PMD_DRV_LOG(ERR,
			"Name for the ena_com memzone is too long. Port: %d, mz_num: %" PRIu64 "\n",
			data->port_id, adapter->memzone_cnt);
		goto error;
	}
	adapter->memzone_cnt++;

	memzone = rte_memzone_reserve_aligned(z_name, size, socket_id,
		RTE_MEMZONE_IOVA_CONTIG, alignment);
	if (memzone == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate ena_com memzone: %s\n",
			z_name);
		goto error;
	}

	memset(memzone->addr, 0, size);
	*virt_addr = memzone->addr;
	*phys_addr = memzone->iova;

	return memzone;

error:
	*virt_addr = NULL;
	*phys_addr = 0;

	return NULL;
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
RTE_PMD_REGISTER_PARAM_STRING(net_ena,
	ENA_DEVARG_LARGE_LLQ_HDR "=<0|1> "
	ENA_DEVARG_ENABLE_LLQ "=<0|1> "
	ENA_DEVARG_MISS_TXC_TO "=<uint>");
RTE_LOG_REGISTER_SUFFIX(ena_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(ena_logtype_driver, driver, NOTICE);
#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER_SUFFIX(ena_logtype_rx, rx, DEBUG);
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER_SUFFIX(ena_logtype_tx, tx, DEBUG);
#endif
RTE_LOG_REGISTER_SUFFIX(ena_logtype_com, com, WARNING);

/******************************************************************************
 ******************************** AENQ Handlers *******************************
 *****************************************************************************/
static void ena_update_on_link_change(void *adapter_data,
				      struct ena_admin_aenq_entry *aenq_e)
{
	struct rte_eth_dev *eth_dev = adapter_data;
	struct ena_adapter *adapter = eth_dev->data->dev_private;
	struct ena_admin_aenq_link_change_desc *aenq_link_desc;
	uint32_t status;

	aenq_link_desc = (struct ena_admin_aenq_link_change_desc *)aenq_e;

	status = get_ena_admin_aenq_link_change_desc_link_status(aenq_link_desc);
	adapter->link_status = status;

	ena_link_update(eth_dev, 0);
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

static void ena_notification(void *adapter_data,
			     struct ena_admin_aenq_entry *aenq_e)
{
	struct rte_eth_dev *eth_dev = adapter_data;
	struct ena_adapter *adapter = eth_dev->data->dev_private;
	struct ena_admin_ena_hw_hints *hints;

	if (aenq_e->aenq_common_desc.group != ENA_ADMIN_NOTIFICATION)
		PMD_DRV_LOG(WARNING, "Invalid AENQ group: %x. Expected: %x\n",
			aenq_e->aenq_common_desc.group,
			ENA_ADMIN_NOTIFICATION);

	switch (aenq_e->aenq_common_desc.syndrome) {
	case ENA_ADMIN_UPDATE_HINTS:
		hints = (struct ena_admin_ena_hw_hints *)
			(&aenq_e->inline_data_w4);
		ena_update_hints(adapter, hints);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid AENQ notification link state: %d\n",
			aenq_e->aenq_common_desc.syndrome);
	}
}

static void ena_keep_alive(void *adapter_data,
			   __rte_unused struct ena_admin_aenq_entry *aenq_e)
{
	struct rte_eth_dev *eth_dev = adapter_data;
	struct ena_adapter *adapter = eth_dev->data->dev_private;
	struct ena_admin_aenq_keep_alive_desc *desc;
	uint64_t rx_drops;
	uint64_t tx_drops;
	uint64_t rx_overruns;

	adapter->timestamp_wd = rte_get_timer_cycles();

	desc = (struct ena_admin_aenq_keep_alive_desc *)aenq_e;
	rx_drops = ((uint64_t)desc->rx_drops_high << 32) | desc->rx_drops_low;
	tx_drops = ((uint64_t)desc->tx_drops_high << 32) | desc->tx_drops_low;
	rx_overruns = ((uint64_t)desc->rx_overruns_high << 32) | desc->rx_overruns_low;

	adapter->drv_stats->rx_drops = rx_drops;
	adapter->dev_stats.tx_drops = tx_drops;
	adapter->dev_stats.rx_overruns = rx_overruns;
}

/**
 * This handler will called for unknown event group or unimplemented handlers
 **/
static void unimplemented_aenq_handler(__rte_unused void *data,
				       __rte_unused struct ena_admin_aenq_entry *aenq_e)
{
	PMD_DRV_LOG(ERR,
		"Unknown event was received or event with unimplemented handler\n");
}

static struct ena_aenq_handlers aenq_handlers = {
	.handlers = {
		[ENA_ADMIN_LINK_CHANGE] = ena_update_on_link_change,
		[ENA_ADMIN_NOTIFICATION] = ena_notification,
		[ENA_ADMIN_KEEP_ALIVE] = ena_keep_alive
	},
	.unimplemented_handler = unimplemented_aenq_handler
};

/*********************************************************************
 *  Multi-Process communication request handling (in primary)
 *********************************************************************/
static int
ena_mp_primary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	const struct ena_mp_body *req =
		(const struct ena_mp_body *)mp_msg->param;
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev;
	struct ena_mp_body *rsp;
	struct rte_mp_msg mp_rsp;
	struct rte_eth_dev *dev;
	int res = 0;

	rsp = (struct ena_mp_body *)&mp_rsp.param;
	mp_msg_init(&mp_rsp, req->type, req->port_id);

	if (!rte_eth_dev_is_valid_port(req->port_id)) {
		rte_errno = ENODEV;
		res = -rte_errno;
		PMD_DRV_LOG(ERR, "Unknown port %d in request %d\n",
			    req->port_id, req->type);
		goto end;
	}
	dev = &rte_eth_devices[req->port_id];
	adapter = dev->data->dev_private;
	ena_dev = &adapter->ena_dev;

	switch (req->type) {
	case ENA_MP_DEV_STATS_GET:
		res = ena_com_get_dev_basic_stats(ena_dev,
						  &adapter->basic_stats);
		break;
	case ENA_MP_ENI_STATS_GET:
		res = ena_com_get_eni_stats(ena_dev,
			(struct ena_admin_eni_stats *)&adapter->metrics_stats);
		break;
	case ENA_MP_MTU_SET:
		res = ena_com_set_dev_mtu(ena_dev, req->args.mtu);
		break;
	case ENA_MP_IND_TBL_GET:
		res = ena_com_indirect_table_get(ena_dev,
						 adapter->indirect_table);
		break;
	case ENA_MP_IND_TBL_SET:
		res = ena_com_indirect_table_set(ena_dev);
		break;
	case ENA_MP_CUSTOMER_METRICS_GET:
		res = ena_com_get_customer_metrics(ena_dev,
				(char *)adapter->metrics_stats,
				sizeof(uint64_t) * adapter->metrics_num);
		break;
	case ENA_MP_SRD_STATS_GET:
		res = ena_com_get_ena_srd_info(ena_dev,
				(struct ena_admin_ena_srd_info *)&adapter->srd_stats);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown request type %d\n", req->type);
		res = -EINVAL;
		break;
	}

end:
	/* Save processing result in the reply */
	rsp->result = res;
	/* Return just IPC processing status */
	return rte_mp_reply(&mp_rsp, peer);
}
