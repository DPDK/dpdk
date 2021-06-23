/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_ETHDEV_H__
#define __CNXK_ETHDEV_H__

#include <math.h>
#include <stdint.h>

#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_kvargs.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>

#include "roc_api.h"

#define CNXK_ETH_DEV_PMD_VERSION "1.0"

/* Used for struct cnxk_eth_dev::flags */
#define CNXK_LINK_CFG_IN_PROGRESS_F BIT_ULL(0)

/* VLAN tag inserted by NIX_TX_VTAG_ACTION.
 * In Tx space is always reserved for this in FRS.
 */
#define CNXK_NIX_MAX_VTAG_INS	   2
#define CNXK_NIX_MAX_VTAG_ACT_SIZE (4 * CNXK_NIX_MAX_VTAG_INS)

/* ETH_HLEN+ETH_FCS+2*VLAN_HLEN */
#define CNXK_NIX_L2_OVERHEAD (RTE_ETHER_HDR_LEN + \
			      RTE_ETHER_CRC_LEN + \
			      CNXK_NIX_MAX_VTAG_ACT_SIZE)

#define CNXK_NIX_RX_MIN_DESC	    16
#define CNXK_NIX_RX_MIN_DESC_ALIGN  16
#define CNXK_NIX_RX_NB_SEG_MAX	    6
#define CNXK_NIX_RX_DEFAULT_RING_SZ 4096
/* Max supported SQB count */
#define CNXK_NIX_TX_MAX_SQB 512

/* If PTP is enabled additional SEND MEM DESC is required which
 * takes 2 words, hence max 7 iova address are possible
 */
#if defined(RTE_LIBRTE_IEEE1588)
#define CNXK_NIX_TX_NB_SEG_MAX 7
#else
#define CNXK_NIX_TX_NB_SEG_MAX 9
#endif

#define CNXK_NIX_TX_MSEG_SG_DWORDS                                             \
	((RTE_ALIGN_MUL_CEIL(CNXK_NIX_TX_NB_SEG_MAX, 3) / 3) +                 \
	 CNXK_NIX_TX_NB_SEG_MAX)

#define CNXK_NIX_RSS_L3_L4_SRC_DST                                             \
	(ETH_RSS_L3_SRC_ONLY | ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY |     \
	 ETH_RSS_L4_DST_ONLY)

#define CNXK_NIX_RSS_OFFLOAD                                                   \
	(ETH_RSS_PORT | ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP |               \
	 ETH_RSS_SCTP | ETH_RSS_TUNNEL | ETH_RSS_L2_PAYLOAD |                  \
	 CNXK_NIX_RSS_L3_L4_SRC_DST | ETH_RSS_LEVEL_MASK | ETH_RSS_C_VLAN)

#define CNXK_NIX_TX_OFFLOAD_CAPA                                               \
	(DEV_TX_OFFLOAD_MBUF_FAST_FREE | DEV_TX_OFFLOAD_MT_LOCKFREE |          \
	 DEV_TX_OFFLOAD_VLAN_INSERT | DEV_TX_OFFLOAD_QINQ_INSERT |             \
	 DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM | DEV_TX_OFFLOAD_OUTER_UDP_CKSUM |    \
	 DEV_TX_OFFLOAD_TCP_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM |                 \
	 DEV_TX_OFFLOAD_SCTP_CKSUM | DEV_TX_OFFLOAD_TCP_TSO |                  \
	 DEV_TX_OFFLOAD_VXLAN_TNL_TSO | DEV_TX_OFFLOAD_GENEVE_TNL_TSO |        \
	 DEV_TX_OFFLOAD_GRE_TNL_TSO | DEV_TX_OFFLOAD_MULTI_SEGS |              \
	 DEV_TX_OFFLOAD_IPV4_CKSUM)

#define CNXK_NIX_RX_OFFLOAD_CAPA                                               \
	(DEV_RX_OFFLOAD_CHECKSUM | DEV_RX_OFFLOAD_SCTP_CKSUM |                 \
	 DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM | DEV_RX_OFFLOAD_SCATTER |            \
	 DEV_RX_OFFLOAD_JUMBO_FRAME | DEV_RX_OFFLOAD_OUTER_UDP_CKSUM |         \
	 DEV_RX_OFFLOAD_RSS_HASH)

#define RSS_IPV4_ENABLE                                                        \
	(ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 | ETH_RSS_NONFRAG_IPV4_UDP |         \
	 ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_SCTP)

#define RSS_IPV6_ENABLE                                                        \
	(ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6 | ETH_RSS_NONFRAG_IPV6_UDP |         \
	 ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_NONFRAG_IPV6_SCTP)

#define RSS_IPV6_EX_ENABLE                                                     \
	(ETH_RSS_IPV6_EX | ETH_RSS_IPV6_TCP_EX | ETH_RSS_IPV6_UDP_EX)

#define RSS_MAX_LEVELS 3

#define RSS_IPV4_INDEX 0
#define RSS_IPV6_INDEX 1
#define RSS_TCP_INDEX  2
#define RSS_UDP_INDEX  3
#define RSS_SCTP_INDEX 4
#define RSS_DMAC_INDEX 5

/* Default mark value used when none is provided. */
#define CNXK_FLOW_ACTION_FLAG_DEFAULT 0xffff

#define PTYPE_NON_TUNNEL_WIDTH	  16
#define PTYPE_TUNNEL_WIDTH	  12
#define PTYPE_NON_TUNNEL_ARRAY_SZ BIT(PTYPE_NON_TUNNEL_WIDTH)
#define PTYPE_TUNNEL_ARRAY_SZ	  BIT(PTYPE_TUNNEL_WIDTH)
#define PTYPE_ARRAY_SZ                                                         \
	((PTYPE_NON_TUNNEL_ARRAY_SZ + PTYPE_TUNNEL_ARRAY_SZ) * sizeof(uint16_t))
/* Fastpath lookup */
#define CNXK_NIX_FASTPATH_LOOKUP_MEM "cnxk_nix_fastpath_lookup_mem"

#define CNXK_NIX_UDP_TUN_BITMASK                                               \
	((1ull << (PKT_TX_TUNNEL_VXLAN >> 45)) |                               \
	 (1ull << (PKT_TX_TUNNEL_GENEVE >> 45)))

struct cnxk_eth_qconf {
	union {
		struct rte_eth_txconf tx;
		struct rte_eth_rxconf rx;
	} conf;
	struct rte_mempool *mp;
	uint16_t nb_desc;
	uint8_t valid;
};

struct cnxk_eth_dev {
	/* ROC NIX */
	struct roc_nix nix;

	/* ROC RQs, SQs and CQs */
	struct roc_nix_rq *rqs;
	struct roc_nix_sq *sqs;
	struct roc_nix_cq *cqs;

	/* Configured queue count */
	uint16_t nb_rxq;
	uint16_t nb_txq;
	uint8_t configured;

	/* Max macfilter entries */
	uint8_t max_mac_entries;
	bool dmac_filter_enable;

	uint16_t flags;
	uint8_t ptype_disable;
	bool scalar_ena;

	/* Pointer back to rte */
	struct rte_eth_dev *eth_dev;

	/* HW capabilities / Limitations */
	union {
		struct {
			uint64_t cq_min_4k : 1;
		};
		uint64_t hwcap;
	};

	/* Rx and Tx offload capabilities */
	uint64_t rx_offload_capa;
	uint64_t tx_offload_capa;
	uint32_t speed_capa;
	/* Configured Rx and Tx offloads */
	uint64_t rx_offloads;
	uint64_t tx_offloads;
	/* Platform specific offload flags */
	uint16_t rx_offload_flags;
	uint16_t tx_offload_flags;

	/* ETHDEV RSS HF bitmask */
	uint64_t ethdev_rss_hf;

	/* Saved qconf before lf realloc */
	struct cnxk_eth_qconf *tx_qconf;
	struct cnxk_eth_qconf *rx_qconf;

	/* Rx burst for cleanup(Only Primary) */
	eth_rx_burst_t rx_pkt_burst_no_offload;

	/* Default mac address */
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];

	/* LSO Tunnel format indices */
	uint64_t lso_tun_fmt;
};

struct cnxk_eth_rxq_sp {
	struct cnxk_eth_dev *dev;
	struct cnxk_eth_qconf qconf;
	uint16_t qid;
} __plt_cache_aligned;

struct cnxk_eth_txq_sp {
	struct cnxk_eth_dev *dev;
	struct cnxk_eth_qconf qconf;
	uint16_t qid;
} __plt_cache_aligned;

static inline struct cnxk_eth_dev *
cnxk_eth_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

static inline struct cnxk_eth_rxq_sp *
cnxk_eth_rxq_to_sp(void *__rxq)
{
	return ((struct cnxk_eth_rxq_sp *)__rxq) - 1;
}

static inline struct cnxk_eth_txq_sp *
cnxk_eth_txq_to_sp(void *__txq)
{
	return ((struct cnxk_eth_txq_sp *)__txq) - 1;
}

/* Common ethdev ops */
extern struct eth_dev_ops cnxk_eth_dev_ops;

/* Ops */
int cnxk_nix_probe(struct rte_pci_driver *pci_drv,
		   struct rte_pci_device *pci_dev);
int cnxk_nix_remove(struct rte_pci_device *pci_dev);
int cnxk_nix_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu);
int cnxk_nix_mac_addr_add(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr, uint32_t index,
			  uint32_t pool);
void cnxk_nix_mac_addr_del(struct rte_eth_dev *eth_dev, uint32_t index);
int cnxk_nix_mac_addr_set(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr);
int cnxk_nix_promisc_enable(struct rte_eth_dev *eth_dev);
int cnxk_nix_promisc_disable(struct rte_eth_dev *eth_dev);
int cnxk_nix_info_get(struct rte_eth_dev *eth_dev,
		      struct rte_eth_dev_info *dev_info);
int cnxk_nix_configure(struct rte_eth_dev *eth_dev);
int cnxk_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			    uint16_t nb_desc, uint16_t fp_tx_q_sz,
			    const struct rte_eth_txconf *tx_conf);
int cnxk_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			    uint16_t nb_desc, uint16_t fp_rx_q_sz,
			    const struct rte_eth_rxconf *rx_conf,
			    struct rte_mempool *mp);
int cnxk_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qid);
int cnxk_nix_dev_start(struct rte_eth_dev *eth_dev);

uint64_t cnxk_nix_rxq_mbuf_setup(struct cnxk_eth_dev *dev);

/* RSS */
uint32_t cnxk_rss_ethdev_to_nix(struct cnxk_eth_dev *dev, uint64_t ethdev_rss,
				uint8_t rss_level);

/* Link */
void cnxk_nix_toggle_flag_link_cfg(struct cnxk_eth_dev *dev, bool set);
void cnxk_eth_dev_link_status_cb(struct roc_nix *nix,
				 struct roc_nix_link_info *link);
int cnxk_nix_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete);

/* Lookup configuration */
const uint32_t *cnxk_nix_supported_ptypes_get(struct rte_eth_dev *eth_dev);
void *cnxk_nix_fastpath_lookup_mem_get(void);

/* Devargs */
int cnxk_ethdev_parse_devargs(struct rte_devargs *devargs,
			      struct cnxk_eth_dev *dev);

/* Inlines */
static __rte_always_inline uint64_t
cnxk_pktmbuf_detach(struct rte_mbuf *m)
{
	struct rte_mempool *mp = m->pool;
	uint32_t mbuf_size, buf_len;
	struct rte_mbuf *md;
	uint16_t priv_size;
	uint16_t refcount;

	/* Update refcount of direct mbuf */
	md = rte_mbuf_from_indirect(m);
	refcount = rte_mbuf_refcnt_update(md, -1);

	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = (uint32_t)(sizeof(struct rte_mbuf) + priv_size);
	buf_len = rte_pktmbuf_data_room_size(mp);

	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
	m->buf_len = (uint16_t)buf_len;
	rte_pktmbuf_reset_headroom(m);
	m->data_len = 0;
	m->ol_flags = 0;
	m->next = NULL;
	m->nb_segs = 1;

	/* Now indirect mbuf is safe to free */
	rte_pktmbuf_free(m);

	if (refcount == 0) {
		rte_mbuf_refcnt_set(md, 1);
		md->data_len = 0;
		md->ol_flags = 0;
		md->next = NULL;
		md->nb_segs = 1;
		return 0;
	} else {
		return 1;
	}
}

static __rte_always_inline uint64_t
cnxk_nix_prefree_seg(struct rte_mbuf *m)
{
	if (likely(rte_mbuf_refcnt_read(m) == 1)) {
		if (!RTE_MBUF_DIRECT(m))
			return cnxk_pktmbuf_detach(m);

		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	} else if (rte_mbuf_refcnt_update(m, -1) == 0) {
		if (!RTE_MBUF_DIRECT(m))
			return cnxk_pktmbuf_detach(m);

		rte_mbuf_refcnt_set(m, 1);
		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	}

	/* Mbuf is having refcount more than 1 so need not to be freed */
	return 1;
}

#endif /* __CNXK_ETHDEV_H__ */
