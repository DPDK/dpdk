/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2009-2018 Microsoft Corp.
 * Copyright (c) 2016 Brocade Communications Systems, Inc.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 */

/*
 * Tunable ethdev params
 */
#define HN_MIN_RX_BUF_SIZE	1024
#define HN_MAX_XFER_LEN		2048
#define	HN_MAX_MAC_ADDRS	1
#define HN_MAX_CHANNELS		64

/* Claimed to be 12232B */
#define HN_MTU_MAX		(9 * 1024)

/* Retry interval */
#define HN_CHAN_INTERVAL_US	100

/* Host monitor interval */
#define HN_CHAN_LATENCY_NS	50000

#define HN_TXCOPY_THRESHOLD	512
#define HN_RXCOPY_THRESHOLD	256

#define HN_RX_EXTMBUF_ENABLE	0

/* Buffers need to be aligned */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE - 1)
#endif

struct hn_data;
struct hn_txdesc;

struct hn_stats {
	uint64_t	packets;
	uint64_t	bytes;
	uint64_t	errors;
	uint64_t	ring_full;
	uint64_t	channel_full;
	uint64_t	multicast;
	uint64_t	broadcast;
	/* Size bins in array as RFC 2819, undersized [0], 64 [1], etc */
	uint64_t	size_bins[8];
};

struct hn_tx_queue {
	struct hn_data  *hv;
	struct vmbus_channel *chan;
	uint16_t	port_id;
	uint16_t	queue_id;
	uint32_t	free_thresh;
	struct rte_mempool *txdesc_pool;
	const struct rte_memzone *tx_rndis_mz;
	void		*tx_rndis;
	rte_iova_t	tx_rndis_iova;

	/* Applied packet transmission aggregation limits. */
	uint32_t	agg_szmax;
	uint32_t	agg_pktmax;
	uint32_t	agg_align;

	/* Packet transmission aggregation states */
	struct hn_txdesc *agg_txd;
	uint32_t	agg_pktleft;
	uint32_t	agg_szleft;
	struct rndis_packet_msg *agg_prevpkt;

	struct hn_stats stats;
};

struct hn_rx_queue {
	struct hn_data  *hv;
	struct vmbus_channel *chan;
	struct rte_mempool *mb_pool;
	struct rte_ring *rx_ring;

	rte_spinlock_t ring_lock;
	uint32_t event_sz;
	uint16_t port_id;
	uint16_t queue_id;
	struct hn_stats stats;

	void *event_buf;
	struct hn_rx_bufinfo *rxbuf_info;
	rte_atomic32_t  rxbuf_outstanding;
};


/* multi-packet data from host */
struct hn_rx_bufinfo {
	struct vmbus_channel *chan;
	struct hn_rx_queue *rxq;
	uint64_t	xactid;
	struct rte_mbuf_ext_shared_info shinfo;
} __rte_cache_aligned;

#define HN_INVALID_PORT	UINT16_MAX

struct hn_data {
	struct rte_vmbus_device *vmbus;
	struct hn_rx_queue *primary;
	rte_rwlock_t    vf_lock;
	uint16_t	port_id;
	uint16_t	vf_port;

	uint8_t		vf_present;
	uint8_t		closed;
	uint8_t		vlan_strip;

	uint32_t	link_status;
	uint32_t	link_speed;

	struct rte_mem_resource *rxbuf_res;	/* UIO resource for Rx */
	uint32_t	rxbuf_section_cnt;	/* # of Rx sections */
	uint32_t	rx_copybreak;
	uint32_t	rx_extmbuf_enable;
	uint16_t	max_queues;		/* Max available queues */
	uint16_t	num_queues;
	uint64_t	rss_offloads;

	rte_spinlock_t	chim_lock;
	struct rte_mem_resource *chim_res;	/* UIO resource for Tx */
	struct rte_bitmap *chim_bmap;		/* Send buffer map */
	void		*chim_bmem;
	uint32_t	tx_copybreak;
	uint32_t	chim_szmax;		/* Max size per buffer */
	uint32_t	chim_cnt;		/* Max packets per buffer */

	uint32_t	latency;
	uint32_t	nvs_ver;
	uint32_t	ndis_ver;
	uint32_t	rndis_agg_size;
	uint32_t	rndis_agg_pkts;
	uint32_t	rndis_agg_align;

	volatile uint32_t  rndis_pending;
	rte_atomic32_t	rndis_req_id;
	uint8_t		rndis_resp[256];

	uint32_t	rss_hash;
	uint8_t		rss_key[40];
	uint16_t	rss_ind[128];

	struct rte_eth_dev_owner owner;

	struct vmbus_channel *channels[HN_MAX_CHANNELS];
};

static inline struct vmbus_channel *
hn_primary_chan(const struct hn_data *hv)
{
	return hv->channels[0];
}

uint32_t hn_process_events(struct hn_data *hv, uint16_t queue_id,
		       uint32_t tx_limit);

uint16_t hn_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts);
uint16_t hn_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		      uint16_t nb_pkts);

int	hn_chim_init(struct rte_eth_dev *dev);
void	hn_chim_uninit(struct rte_eth_dev *dev);
int	hn_dev_link_update(struct rte_eth_dev *dev, int wait);
int	hn_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			      uint16_t nb_desc, unsigned int socket_id,
			      const struct rte_eth_txconf *tx_conf);
void	hn_dev_tx_queue_release(void *arg);
void	hn_dev_tx_queue_info(struct rte_eth_dev *dev, uint16_t queue_idx,
			     struct rte_eth_txq_info *qinfo);
int	hn_dev_tx_done_cleanup(void *arg, uint32_t free_cnt);
int	hn_dev_tx_descriptor_status(void *arg, uint16_t offset);

struct hn_rx_queue *hn_rx_queue_alloc(struct hn_data *hv,
				      uint16_t queue_id,
				      unsigned int socket_id);
int	hn_dev_rx_queue_setup(struct rte_eth_dev *dev,
			      uint16_t queue_idx, uint16_t nb_desc,
			      unsigned int socket_id,
			      const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mp);
void	hn_dev_rx_queue_info(struct rte_eth_dev *dev, uint16_t queue_id,
			     struct rte_eth_rxq_info *qinfo);
void	hn_dev_rx_queue_release(void *arg);
uint32_t hn_dev_rx_queue_count(struct rte_eth_dev *dev, uint16_t queue_id);
int	hn_dev_rx_queue_status(void *rxq, uint16_t offset);
void	hn_dev_free_queues(struct rte_eth_dev *dev);

/* Check if VF is attached */
static inline bool
hn_vf_attached(const struct hn_data *hv)
{
	return hv->vf_port != HN_INVALID_PORT;
}

/*
 * Get VF device for existing netvsc device
 * Assumes vf_lock is held.
 */
static inline struct rte_eth_dev *
hn_get_vf_dev(const struct hn_data *hv)
{
	uint16_t vf_port = hv->vf_port;

	if (vf_port == HN_INVALID_PORT)
		return NULL;
	else
		return &rte_eth_devices[vf_port];
}

int	hn_vf_info_get(struct hn_data *hv,
		       struct rte_eth_dev_info *info);
int	hn_vf_add(struct rte_eth_dev *dev, struct hn_data *hv);
int	hn_vf_configure(struct rte_eth_dev *dev,
			const struct rte_eth_conf *dev_conf);
const uint32_t *hn_vf_supported_ptypes(struct rte_eth_dev *dev);
int	hn_vf_start(struct rte_eth_dev *dev);
void	hn_vf_reset(struct rte_eth_dev *dev);
int	hn_vf_close(struct rte_eth_dev *dev);
int	hn_vf_stop(struct rte_eth_dev *dev);

int	hn_vf_allmulticast_enable(struct rte_eth_dev *dev);
int	hn_vf_allmulticast_disable(struct rte_eth_dev *dev);
int	hn_vf_promiscuous_enable(struct rte_eth_dev *dev);
int	hn_vf_promiscuous_disable(struct rte_eth_dev *dev);
int	hn_vf_mc_addr_list(struct rte_eth_dev *dev,
			   struct rte_ether_addr *mc_addr_set,
			   uint32_t nb_mc_addr);

int	hn_vf_tx_queue_setup(struct rte_eth_dev *dev,
			     uint16_t queue_idx, uint16_t nb_desc,
			     unsigned int socket_id,
			     const struct rte_eth_txconf *tx_conf);
void	hn_vf_tx_queue_release(struct hn_data *hv, uint16_t queue_id);
int	hn_vf_tx_queue_status(struct hn_data *hv, uint16_t queue_id, uint16_t offset);

int	hn_vf_rx_queue_setup(struct rte_eth_dev *dev,
			     uint16_t queue_idx, uint16_t nb_desc,
			     unsigned int socket_id,
			     const struct rte_eth_rxconf *rx_conf,
			     struct rte_mempool *mp);
void	hn_vf_rx_queue_release(struct hn_data *hv, uint16_t queue_id);

int	hn_vf_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int	hn_vf_stats_reset(struct rte_eth_dev *dev);
int	hn_vf_xstats_get_names(struct rte_eth_dev *dev,
			       struct rte_eth_xstat_name *xstats_names,
			       unsigned int size);
int	hn_vf_xstats_get(struct rte_eth_dev *dev,
			 struct rte_eth_xstat *xstats,
			 unsigned int offset, unsigned int n);
int	hn_vf_xstats_reset(struct rte_eth_dev *dev);
int	hn_vf_rss_hash_update(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf);
int	hn_vf_reta_hash_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);
