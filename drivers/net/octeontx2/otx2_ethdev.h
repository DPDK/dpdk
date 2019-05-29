/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_ETHDEV_H__
#define __OTX2_ETHDEV_H__

#include <stdint.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_kvargs.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_string_fns.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_irq.h"
#include "otx2_mempool.h"
#include "otx2_rx.h"
#include "otx2_tm.h"
#include "otx2_tx.h"

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
#define NIX_SQB_LIST_SPACE		2
#define NIX_RSS_RETA_SIZE_MAX		256
/* Group 0 will be used for RSS, 1 -7 will be used for rte_flow RSS action*/
#define NIX_RSS_GRPS			8
#define NIX_HASH_KEY_SIZE		48 /* 352 Bits */
#define NIX_RSS_RETA_SIZE		64
#define	NIX_RX_MIN_DESC			16
#define NIX_RX_MIN_DESC_ALIGN		16
#define NIX_RX_NB_SEG_MAX		6
#define NIX_CQ_ENTRY_SZ			128
#define NIX_CQ_ALIGN			512
#define NIX_SQB_LOWER_THRESH		90
#define LMT_SLOT_MASK			0x7f

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

#define NIX_DEFAULT_RSS_CTX_GROUP  0
#define NIX_DEFAULT_RSS_MCAM_IDX  -1

enum nix_q_size_e {
	nix_q_size_16,	/* 16 entries */
	nix_q_size_64,	/* 64 entries */
	nix_q_size_256,
	nix_q_size_1K,
	nix_q_size_4K,
	nix_q_size_16K,
	nix_q_size_64K,
	nix_q_size_256K,
	nix_q_size_1M,	/* Million entries */
	nix_q_size_max
};

struct otx2_qint {
	struct rte_eth_dev *eth_dev;
	uint8_t qintx;
};

struct otx2_rss_info {
	uint64_t nix_rss;
	uint32_t flowkey_cfg;
	uint16_t rss_size;
	uint8_t rss_grps;
	uint8_t alg_idx; /* Selected algo index */
	uint16_t ind_tbl[NIX_RSS_RETA_SIZE_MAX];
	uint8_t key[NIX_HASH_KEY_SIZE];
};

struct otx2_eth_qconf {
	union {
		struct rte_eth_txconf tx;
		struct rte_eth_rxconf rx;
	} conf;
	void *mempool;
	uint32_t socket_id;
	uint16_t nb_desc;
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
	uint16_t txschq[NIX_TXSCH_LVL_CNT];
	uint16_t txschq_contig[NIX_TXSCH_LVL_CNT];
	uint16_t txschq_index[NIX_TXSCH_LVL_CNT];
	uint16_t txschq_contig_index[NIX_TXSCH_LVL_CNT];
	/* Dis-contiguous queues */
	uint16_t txschq_list[NIX_TXSCH_LVL_CNT][MAX_TXSCHQ_PER_FUNC];
	/* Contiguous queues */
	uint16_t txschq_contig_list[NIX_TXSCH_LVL_CNT][MAX_TXSCHQ_PER_FUNC];
	uint16_t otx2_tm_root_lvl;
	uint16_t tm_flags;
	uint16_t tm_leaf_cnt;
	struct otx2_nix_tm_node_list node_list;
	struct otx2_nix_tm_shaper_profile_list shaper_profile_list;
	struct otx2_rss_info rss_info;
	uint32_t txmap[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	uint32_t rxmap[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	struct otx2_npc_flow_info npc_flow;
	struct otx2_eth_qconf *tx_qconf;
	struct otx2_eth_qconf *rx_qconf;
	struct rte_eth_dev *eth_dev;
} __rte_cache_aligned;

struct otx2_eth_txq {
	uint64_t cmd[8];
	int64_t fc_cache_pkts;
	uint64_t *fc_mem;
	void *lmt_addr;
	rte_iova_t io_addr;
	rte_iova_t fc_iova;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
	MARKER slow_path_start;
	uint16_t nb_sqb_bufs;
	uint16_t sq;
	uint64_t offloads;
	struct otx2_eth_dev *dev;
	struct rte_mempool *sqb_pool;
	struct otx2_eth_qconf qconf;
} __rte_cache_aligned;

struct otx2_eth_rxq {
	uint64_t mbuf_initializer;
	uint64_t data_off;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t rq;
	struct otx2_timesync_info *tstamp;
	MARKER slow_path_start;
	uint64_t aura;
	uint64_t offloads;
	uint32_t qlen;
	struct rte_mempool *pool;
	enum nix_q_size_e qsize;
	struct rte_eth_dev *eth_dev;
	struct otx2_eth_qconf qconf;
} __rte_cache_aligned;

static inline struct otx2_eth_dev *
otx2_eth_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

/* Ops */
void otx2_nix_info_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_dev_info *dev_info);

void otx2_nix_promisc_config(struct rte_eth_dev *eth_dev, int en);
void otx2_nix_promisc_enable(struct rte_eth_dev *eth_dev);
void otx2_nix_promisc_disable(struct rte_eth_dev *eth_dev);
void otx2_nix_allmulticast_enable(struct rte_eth_dev *eth_dev);
void otx2_nix_allmulticast_disable(struct rte_eth_dev *eth_dev);
int otx2_nix_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qidx);
int otx2_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx);
uint64_t otx2_nix_rxq_mbuf_setup(struct otx2_eth_dev *dev, uint16_t port_id);

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

/* RSS */
void otx2_nix_rss_set_key(struct otx2_eth_dev *dev,
			  uint8_t *key, uint32_t key_len);
uint32_t otx2_rss_ethdev_to_nix(struct otx2_eth_dev *dev,
				uint64_t ethdev_rss, uint8_t rss_level);
int otx2_rss_set_hf(struct otx2_eth_dev *dev,
		    uint32_t flowkey_cfg, uint8_t *alg_idx,
		    uint8_t group, int mcam_index);
int otx2_nix_rss_tbl_init(struct otx2_eth_dev *dev, uint8_t group,
			  uint16_t *ind_tbl);
int otx2_nix_rss_config(struct rte_eth_dev *eth_dev);

int otx2_nix_dev_reta_update(struct rte_eth_dev *eth_dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size);
int otx2_nix_dev_reta_query(struct rte_eth_dev *eth_dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size);
int otx2_nix_rss_hash_update(struct rte_eth_dev *eth_dev,
			     struct rte_eth_rss_conf *rss_conf);

int otx2_nix_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
			       struct rte_eth_rss_conf *rss_conf);

/* CGX */
int otx2_cgx_rxtx_start(struct otx2_eth_dev *dev);
int otx2_cgx_rxtx_stop(struct otx2_eth_dev *dev);
int otx2_cgx_mac_addr_set(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr);

/* Mac address handling */
int otx2_nix_mac_addr_set(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr);
int otx2_nix_mac_addr_get(struct rte_eth_dev *eth_dev, uint8_t *addr);
int otx2_nix_mac_addr_add(struct rte_eth_dev *eth_dev,
			  struct rte_ether_addr *addr,
			  uint32_t index, uint32_t pool);
void otx2_nix_mac_addr_del(struct rte_eth_dev *eth_dev, uint32_t index);
int otx2_cgx_mac_max_entries_get(struct otx2_eth_dev *dev);

/* Devargs */
int otx2_ethdev_parse_devargs(struct rte_devargs *devargs,
			      struct otx2_eth_dev *dev);

/* Rx and Tx routines */
void otx2_nix_form_default_desc(struct otx2_eth_txq *txq);

#endif /* __OTX2_ETHDEV_H__ */
