/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_ETHDEV_H__
#define __OTX2_ETHDEV_H__

#include <stdint.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_kvargs.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_irq.h"
#include "otx2_mempool.h"
#include "otx2_rx.h"

#define OTX2_ETH_DEV_PMD_VERSION	"1.0"

/* Ethdev HWCAP and Fixup flags. Use from MSB bits to avoid conflict with dev */

/* Minimum CQ size should be 4K */
#define OTX2_FIXUP_F_MIN_4K_Q		BIT_ULL(63)
#define otx2_ethdev_fixup_is_min_4k_q(dev)	\
				((dev)->hwcap & OTX2_FIXUP_F_MIN_4K_Q)
/* Limit CQ being full */
#define OTX2_FIXUP_F_LIMIT_CQ_FULL	BIT_ULL(62)
#define otx2_ethdev_fixup_is_limit_cq_full(dev) \
				((dev)->hwcap & OTX2_FIXUP_F_LIMIT_CQ_FULL)

/* Used for struct otx2_eth_dev::flags */
#define OTX2_LINK_CFG_IN_PROGRESS_F	BIT_ULL(0)

/* VLAN tag inserted by NIX_TX_VTAG_ACTION.
 * In Tx space is always reserved for this in FRS.
 */
#define NIX_MAX_VTAG_INS		2
#define NIX_MAX_VTAG_ACT_SIZE		(4 * NIX_MAX_VTAG_INS)

/* ETH_HLEN+ETH_FCS+2*VLAN_HLEN */
#define NIX_L2_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + 8)

/* HW config of frame size doesn't include FCS */
#define NIX_MAX_HW_FRS			9212
#define NIX_MIN_HW_FRS			60

/* Since HW FRS includes NPC VTAG insertion space, user has reduced FRS */
#define NIX_MAX_FRS	\
	(NIX_MAX_HW_FRS + RTE_ETHER_CRC_LEN - NIX_MAX_VTAG_ACT_SIZE)

#define NIX_MIN_FRS	\
	(NIX_MIN_HW_FRS + RTE_ETHER_CRC_LEN)

#define NIX_MAX_MTU	\
	(NIX_MAX_FRS - NIX_L2_OVERHEAD)

#define NIX_MAX_SQB			512
#define NIX_MIN_SQB			32
/* Group 0 will be used for RSS, 1 -7 will be used for rte_flow RSS action*/
#define NIX_RSS_GRPS			8
#define NIX_HASH_KEY_SIZE		48 /* 352 Bits */
#define NIX_RSS_RETA_SIZE		64
#define	NIX_RX_MIN_DESC			16
#define NIX_RX_MIN_DESC_ALIGN		16
#define NIX_RX_NB_SEG_MAX		6
#define NIX_CQ_ENTRY_SZ			128

/* If PTP is enabled additional SEND MEM DESC is required which
 * takes 2 words, hence max 7 iova address are possible
 */
#if defined(RTE_LIBRTE_IEEE1588)
#define NIX_TX_NB_SEG_MAX		7
#else
#define NIX_TX_NB_SEG_MAX		9
#endif

#define CQ_OP_STAT_OP_ERR	63
#define CQ_OP_STAT_CQ_ERR	46

#define OP_ERR			BIT_ULL(CQ_OP_STAT_OP_ERR)
#define CQ_ERR			BIT_ULL(CQ_OP_STAT_CQ_ERR)

#define NIX_RSS_OFFLOAD		(ETH_RSS_PORT | ETH_RSS_IP | ETH_RSS_UDP |\
				 ETH_RSS_TCP | ETH_RSS_SCTP | \
				 ETH_RSS_TUNNEL | ETH_RSS_L2_PAYLOAD)

#define NIX_TX_OFFLOAD_CAPA ( \
	DEV_TX_OFFLOAD_MBUF_FAST_FREE	| \
	DEV_TX_OFFLOAD_MT_LOCKFREE	| \
	DEV_TX_OFFLOAD_VLAN_INSERT	| \
	DEV_TX_OFFLOAD_QINQ_INSERT	| \
	DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM | \
	DEV_TX_OFFLOAD_OUTER_UDP_CKSUM  | \
	DEV_TX_OFFLOAD_TCP_CKSUM	| \
	DEV_TX_OFFLOAD_UDP_CKSUM	| \
	DEV_TX_OFFLOAD_SCTP_CKSUM	| \
	DEV_TX_OFFLOAD_MULTI_SEGS	| \
	DEV_TX_OFFLOAD_IPV4_CKSUM)

#define NIX_RX_OFFLOAD_CAPA ( \
	DEV_RX_OFFLOAD_CHECKSUM		| \
	DEV_RX_OFFLOAD_SCTP_CKSUM	| \
	DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM | \
	DEV_RX_OFFLOAD_SCATTER		| \
	DEV_RX_OFFLOAD_JUMBO_FRAME	| \
	DEV_RX_OFFLOAD_OUTER_UDP_CKSUM | \
	DEV_RX_OFFLOAD_VLAN_STRIP | \
	DEV_RX_OFFLOAD_VLAN_FILTER | \
	DEV_RX_OFFLOAD_QINQ_STRIP | \
	DEV_RX_OFFLOAD_TIMESTAMP)

struct otx2_qint {
	struct rte_eth_dev *eth_dev;
	uint8_t qintx;
};

struct otx2_rss_info {
	uint16_t rss_size;
	uint8_t rss_grps;
};

struct otx2_npc_flow_info {
	uint16_t channel; /*rx channel */
	uint16_t flow_prealloc_size;
	uint16_t flow_max_priority;
};

struct otx2_eth_dev {
	OTX2_DEV; /* Base class */
	MARKER otx2_eth_dev_data_start;
	uint16_t sqb_size;
	uint16_t rx_chan_base;
	uint16_t tx_chan_base;
	uint8_t rx_chan_cnt;
	uint8_t tx_chan_cnt;
	uint8_t lso_tsov4_idx;
	uint8_t lso_tsov6_idx;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t max_mac_entries;
	uint8_t lf_tx_stats;
	uint8_t lf_rx_stats;
	uint16_t flags;
	uint16_t cints;
	uint16_t qints;
	uint8_t configured;
	uint8_t configured_qints;
	uint8_t configured_nb_rx_qs;
	uint8_t configured_nb_tx_qs;
	uint16_t nix_msixoff;
	uintptr_t base;
	uintptr_t lmt_addr;
	uint16_t scalar_ena;
	uint16_t max_sqb_count;
	uint16_t rx_offload_flags; /* Selected Rx offload flags(NIX_RX_*_F) */
	uint64_t rx_offloads;
	uint16_t tx_offload_flags; /* Selected Tx offload flags(NIX_TX_*_F) */
	uint64_t tx_offloads;
	uint64_t rx_offload_capa;
	uint64_t tx_offload_capa;
	struct otx2_qint qints_mem[RTE_MAX_QUEUES_PER_PORT];
	struct otx2_rss_info rss_info;
	uint32_t txmap[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	uint32_t rxmap[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	struct otx2_npc_flow_info npc_flow;
	struct rte_eth_dev *eth_dev;
} __rte_cache_aligned;

static inline struct otx2_eth_dev *
otx2_eth_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

/* Ops */
void otx2_nix_info_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_dev_info *dev_info);

/* Link */
void otx2_nix_toggle_flag_link_cfg(struct otx2_eth_dev *dev, bool set);
int otx2_nix_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete);
void otx2_eth_dev_link_status_update(struct otx2_dev *dev,
				     struct cgx_link_user_info *link);

/* IRQ */
int otx2_nix_register_irqs(struct rte_eth_dev *eth_dev);
int oxt2_nix_register_queue_irqs(struct rte_eth_dev *eth_dev);
void otx2_nix_unregister_irqs(struct rte_eth_dev *eth_dev);
void oxt2_nix_unregister_queue_irqs(struct rte_eth_dev *eth_dev);

/* Debug */
int otx2_nix_reg_dump(struct otx2_eth_dev *dev, uint64_t *data);
int otx2_nix_dev_get_reg(struct rte_eth_dev *eth_dev,
			 struct rte_dev_reg_info *regs);
int otx2_nix_queues_ctx_dump(struct rte_eth_dev *eth_dev);
void otx2_nix_cqe_dump(const struct nix_cqe_hdr_s *cq);

/* Stats */
int otx2_nix_dev_stats_get(struct rte_eth_dev *eth_dev,
			   struct rte_eth_stats *stats);
void otx2_nix_dev_stats_reset(struct rte_eth_dev *eth_dev);

int otx2_nix_queue_stats_mapping(struct rte_eth_dev *dev,
				 uint16_t queue_id, uint8_t stat_idx,
				 uint8_t is_rx);
int otx2_nix_xstats_get(struct rte_eth_dev *eth_dev,
			struct rte_eth_xstat *xstats, unsigned int n);
int otx2_nix_xstats_get_names(struct rte_eth_dev *eth_dev,
			      struct rte_eth_xstat_name *xstats_names,
			      unsigned int limit);
void otx2_nix_xstats_reset(struct rte_eth_dev *eth_dev);

int otx2_nix_xstats_get_by_id(struct rte_eth_dev *eth_dev,
			      const uint64_t *ids,
			      uint64_t *values, unsigned int n);
int otx2_nix_xstats_get_names_by_id(struct rte_eth_dev *eth_dev,
				    struct rte_eth_xstat_name *xstats_names,
				    const uint64_t *ids, unsigned int limit);

/* CGX */
int otx2_cgx_rxtx_start(struct otx2_eth_dev *dev);
int otx2_cgx_rxtx_stop(struct otx2_eth_dev *dev);
int otx2_cgx_mac_addr_set(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr);

/* Mac address handling */
int otx2_nix_mac_addr_get(struct rte_eth_dev *eth_dev, uint8_t *addr);
int otx2_cgx_mac_max_entries_get(struct otx2_eth_dev *dev);

/* Devargs */
int otx2_ethdev_parse_devargs(struct rte_devargs *devargs,
			      struct otx2_eth_dev *dev);

#endif /* __OTX2_ETHDEV_H__ */
