/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _E1000_ETHDEV_H_
#define _E1000_ETHDEV_H_

/* need update link, bit flag */
#define E1000_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)
#define E1000_FLAG_MAILBOX          (uint32_t)(1 << 1)

/*
 * Defines that were not part of e1000_hw.h as they are not used by the FreeBSD
 * driver.
 */
#define E1000_ADVTXD_POPTS_TXSM     0x00000200 /* L4 Checksum offload request */
#define E1000_ADVTXD_POPTS_IXSM     0x00000100 /* IP Checksum offload request */
#define E1000_ADVTXD_TUCMD_L4T_RSV  0x00001800 /* L4 Packet TYPE of Reserved */
#define E1000_RXD_STAT_TMST         0x10000    /* Timestamped Packet indication */
#define E1000_RXD_ERR_CKSUM_BIT     29
#define E1000_RXD_ERR_CKSUM_MSK     3
#define E1000_ADVTXD_MACLEN_SHIFT   9          /* Bit shift for l2_len */
#define E1000_CTRL_EXT_EXTEND_VLAN  (1<<26)    /* EXTENDED VLAN */
#define IGB_VFTA_SIZE 128

#define IGB_MAX_RX_QUEUE_NUM           8
#define IGB_MAX_RX_QUEUE_NUM_82576     16

#define E1000_SYN_FILTER_ENABLE        0x00000001 /* syn filter enable field */
#define E1000_SYN_FILTER_QUEUE         0x0000000E /* syn filter queue field */
#define E1000_SYN_FILTER_QUEUE_SHIFT   1          /* syn filter queue field */
#define E1000_RFCTL_SYNQFP             0x00080000 /* SYNQFP in RFCTL register */

#define E1000_ETQF_ETHERTYPE           0x0000FFFF
#define E1000_ETQF_QUEUE               0x00070000
#define E1000_ETQF_QUEUE_SHIFT         16
#define E1000_MAX_ETQF_FILTERS         8

#define E1000_IMIR_DSTPORT             0x0000FFFF
#define E1000_IMIR_PRIORITY            0xE0000000
#define E1000_MAX_TTQF_FILTERS         8
#define E1000_2TUPLE_MAX_PRI           7

#define E1000_MAX_FLEX_FILTERS           8
#define E1000_MAX_FHFT                   4
#define E1000_MAX_FHFT_EXT               4
#define E1000_FHFT_SIZE_IN_DWD           64
#define E1000_MAX_FLEX_FILTER_PRI        7
#define E1000_MAX_FLEX_FILTER_LEN        128
#define E1000_MAX_FLEX_FILTER_DWDS \
	(E1000_MAX_FLEX_FILTER_LEN / sizeof(uint32_t))
#define E1000_FLEX_FILTERS_MASK_SIZE \
	(E1000_MAX_FLEX_FILTER_DWDS / 4)
#define E1000_FHFT_QUEUEING_LEN          0x0000007F
#define E1000_FHFT_QUEUEING_QUEUE        0x00000700
#define E1000_FHFT_QUEUEING_PRIO         0x00070000
#define E1000_FHFT_QUEUEING_OFFSET       0xFC
#define E1000_FHFT_QUEUEING_QUEUE_SHIFT  8
#define E1000_FHFT_QUEUEING_PRIO_SHIFT   16
#define E1000_WUFC_FLEX_HQ               0x00004000

#define E1000_SPQF_SRCPORT               0x0000FFFF

#define E1000_MAX_FTQF_FILTERS           8
#define E1000_FTQF_PROTOCOL_MASK         0x000000FF
#define E1000_FTQF_5TUPLE_MASK_SHIFT     28
#define E1000_FTQF_QUEUE_MASK            0x03ff0000
#define E1000_FTQF_QUEUE_SHIFT           16
#define E1000_FTQF_QUEUE_ENABLE          0x00000100

#define IGB_RSS_OFFLOAD_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_IPV6_EX | \
	ETH_RSS_IPV6_TCP_EX | \
	ETH_RSS_IPV6_UDP_EX)

/* structure for interrupt relative data */
struct e1000_interrupt {
	uint32_t flags;
	uint32_t mask;
};

/* local vfta copy */
struct e1000_vfta {
	uint32_t vfta[IGB_VFTA_SIZE];
};

/*
 * VF data which used by PF host only
 */
#define E1000_MAX_VF_MC_ENTRIES         30
struct e1000_vf_info {
	uint8_t vf_mac_addresses[ETHER_ADDR_LEN];
	uint16_t vf_mc_hashes[E1000_MAX_VF_MC_ENTRIES];
	uint16_t num_vf_mc_hashes;
	uint16_t default_vf_vlan_id;
	uint16_t vlans_enabled;
	uint16_t pf_qos;
	uint16_t vlan_count;
	uint16_t tx_rate;
};

TAILQ_HEAD(e1000_flex_filter_list, e1000_flex_filter);

struct e1000_flex_filter_info {
	uint16_t len;
	uint32_t dwords[E1000_MAX_FLEX_FILTER_DWDS]; /* flex bytes in dword. */
	/* if mask bit is 1b, do not compare corresponding byte in dwords. */
	uint8_t mask[E1000_FLEX_FILTERS_MASK_SIZE];
	uint8_t priority;
};

/* Flex filter structure */
struct e1000_flex_filter {
	TAILQ_ENTRY(e1000_flex_filter) entries;
	uint16_t index; /* index of flex filter */
	struct e1000_flex_filter_info filter_info;
	uint16_t queue; /* rx queue assigned to */
};

TAILQ_HEAD(e1000_5tuple_filter_list, e1000_5tuple_filter);
TAILQ_HEAD(e1000_2tuple_filter_list, e1000_2tuple_filter);

struct e1000_5tuple_filter_info {
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t dst_port;
	uint16_t src_port;
	uint8_t proto;           /* l4 protocol. */
	/* the packet matched above 5tuple and contain any set bit will hit this filter. */
	uint8_t tcp_flags;
	uint8_t priority;        /* seven levels (001b-111b), 111b is highest,
				      used when more than one filter matches. */
	uint8_t dst_ip_mask:1,   /* if mask is 1b, do not compare dst ip. */
		src_ip_mask:1,   /* if mask is 1b, do not compare src ip. */
		dst_port_mask:1, /* if mask is 1b, do not compare dst port. */
		src_port_mask:1, /* if mask is 1b, do not compare src port. */
		proto_mask:1;    /* if mask is 1b, do not compare protocol. */
};

struct e1000_2tuple_filter_info {
	uint16_t dst_port;
	uint8_t proto;           /* l4 protocol. */
	/* the packet matched above 2tuple and contain any set bit will hit this filter. */
	uint8_t tcp_flags;
	uint8_t priority;        /* seven levels (001b-111b), 111b is highest,
				      used when more than one filter matches. */
	uint8_t dst_ip_mask:1,   /* if mask is 1b, do not compare dst ip. */
		src_ip_mask:1,   /* if mask is 1b, do not compare src ip. */
		dst_port_mask:1, /* if mask is 1b, do not compare dst port. */
		src_port_mask:1, /* if mask is 1b, do not compare src port. */
		proto_mask:1;    /* if mask is 1b, do not compare protocol. */
};

/* 5tuple filter structure */
struct e1000_5tuple_filter {
	TAILQ_ENTRY(e1000_5tuple_filter) entries;
	uint16_t index;       /* the index of 5tuple filter */
	struct e1000_5tuple_filter_info filter_info;
	uint16_t queue;       /* rx queue assigned to */
};

/* 2tuple filter structure */
struct e1000_2tuple_filter {
	TAILQ_ENTRY(e1000_2tuple_filter) entries;
	uint16_t index;         /* the index of 2tuple filter */
	struct e1000_2tuple_filter_info filter_info;
	uint16_t queue;       /* rx queue assigned to */
};

/*
 * Structure to store filters' info.
 */
struct e1000_filter_info {
	uint8_t ethertype_mask; /* Bit mask for every used ethertype filter */
	/* store used ethertype filters*/
	uint16_t ethertype_filters[E1000_MAX_ETQF_FILTERS];
	uint8_t flex_mask;	/* Bit mask for every used flex filter */
	struct e1000_flex_filter_list flex_list;
	/* Bit mask for every used 5tuple filter */
	uint8_t fivetuple_mask;
	struct e1000_5tuple_filter_list fivetuple_list;
	/* Bit mask for every used 2tuple filter */
	uint8_t twotuple_mask;
	struct e1000_2tuple_filter_list twotuple_list;
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct e1000_adapter {
	struct e1000_hw         hw;
	struct e1000_hw_stats   stats;
	struct e1000_interrupt  intr;
	struct e1000_vfta       shadow_vfta;
	struct e1000_vf_info    *vfdata;
	struct e1000_filter_info filter;
};

#define E1000_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct e1000_adapter *)adapter)->hw)

#define E1000_DEV_PRIVATE_TO_STATS(adapter) \
	(&((struct e1000_adapter *)adapter)->stats)

#define E1000_DEV_PRIVATE_TO_INTR(adapter) \
	(&((struct e1000_adapter *)adapter)->intr)

#define E1000_DEV_PRIVATE_TO_VFTA(adapter) \
	(&((struct e1000_adapter *)adapter)->shadow_vfta)

#define E1000_DEV_PRIVATE_TO_P_VFDATA(adapter) \
        (&((struct e1000_adapter *)adapter)->vfdata)

#define E1000_DEV_PRIVATE_TO_FILTER_INFO(adapter) \
	(&((struct e1000_adapter *)adapter)->filter)

/*
 * RX/TX IGB function prototypes
 */
void eth_igb_tx_queue_release(void *txq);
void eth_igb_rx_queue_release(void *rxq);
void igb_dev_clear_queues(struct rte_eth_dev *dev);

int eth_igb_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

uint32_t eth_igb_rx_queue_count(struct rte_eth_dev *dev,
		uint16_t rx_queue_id);

int eth_igb_rx_descriptor_done(void *rx_queue, uint16_t offset);

int eth_igb_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int eth_igb_rx_init(struct rte_eth_dev *dev);

void eth_igb_tx_init(struct rte_eth_dev *dev);

uint16_t eth_igb_xmit_pkts(void *txq, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t eth_igb_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t eth_igb_recv_scattered_pkts(void *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

int eth_igb_rss_hash_update(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf);

int eth_igb_rss_hash_conf_get(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf);

int eth_igbvf_rx_init(struct rte_eth_dev *dev);

void eth_igbvf_tx_init(struct rte_eth_dev *dev);

/*
 * misc function prototypes
 */
void igb_pf_host_init(struct rte_eth_dev *eth_dev);

void igb_pf_mbx_process(struct rte_eth_dev *eth_dev);

int igb_pf_host_configure(struct rte_eth_dev *eth_dev);

/*
 * RX/TX EM function prototypes
 */
void eth_em_tx_queue_release(void *txq);
void eth_em_rx_queue_release(void *rxq);

void em_dev_clear_queues(struct rte_eth_dev *dev);

int eth_em_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

uint32_t eth_em_rx_queue_count(struct rte_eth_dev *dev,
		uint16_t rx_queue_id);

int eth_em_rx_descriptor_done(void *rx_queue, uint16_t offset);

int eth_em_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int eth_em_rx_init(struct rte_eth_dev *dev);

void eth_em_tx_init(struct rte_eth_dev *dev);

uint16_t eth_em_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t eth_em_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t eth_em_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

#endif /* _E1000_ETHDEV_H_ */
