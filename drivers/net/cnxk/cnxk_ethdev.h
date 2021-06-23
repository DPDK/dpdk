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
#define CNXK_NIX_L2_OVERHEAD (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + 8)

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

#define PTYPE_NON_TUNNEL_WIDTH	  16
#define PTYPE_TUNNEL_WIDTH	  12
#define PTYPE_NON_TUNNEL_ARRAY_SZ BIT(PTYPE_NON_TUNNEL_WIDTH)
#define PTYPE_TUNNEL_ARRAY_SZ	  BIT(PTYPE_TUNNEL_WIDTH)
#define PTYPE_ARRAY_SZ                                                         \
	((PTYPE_NON_TUNNEL_ARRAY_SZ + PTYPE_TUNNEL_ARRAY_SZ) * sizeof(uint16_t))
/* Fastpath lookup */
#define CNXK_NIX_FASTPATH_LOOKUP_MEM "cnxk_nix_fastpath_lookup_mem"

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

uint64_t cnxk_nix_rxq_mbuf_setup(struct cnxk_eth_dev *dev);

/* RSS */
uint32_t cnxk_rss_ethdev_to_nix(struct cnxk_eth_dev *dev, uint64_t ethdev_rss,
				uint8_t rss_level);

/* Link */
void cnxk_eth_dev_link_status_cb(struct roc_nix *nix,
				 struct roc_nix_link_info *link);
int cnxk_nix_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete);

/* Lookup configuration */
const uint32_t *cnxk_nix_supported_ptypes_get(struct rte_eth_dev *eth_dev);
void *cnxk_nix_fastpath_lookup_mem_get(void);

/* Devargs */
int cnxk_ethdev_parse_devargs(struct rte_devargs *devargs,
			      struct cnxk_eth_dev *dev);

#endif /* __CNXK_ETHDEV_H__ */
