/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef RTE_PMD_MLX5_RXTX_H_
#define RTE_PMD_MLX5_RXTX_H_

#include <stddef.h>
#include <stdint.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5_hw.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

struct mlx5_rxq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
#endif
	uint64_t idropped; /**< Total of packets dropped when RX ring full. */
	uint64_t rx_nombuf; /**< Total of RX mbuf allocation failures. */
};

struct mlx5_txq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
#endif
	uint64_t odropped; /**< Total of packets not sent when TX ring full. */
};

/* Flow director queue structure. */
struct fdir_queue {
	struct ibv_qp *qp; /* Associated RX QP. */
	struct ibv_exp_rwq_ind_table *ind_table; /* Indirection table. */
	struct ibv_exp_wq *wq; /* Work queue. */
	struct ibv_cq *cq; /* Completion queue. */
};

struct priv;

/* Compressed CQE context. */
struct rxq_zip {
	uint16_t ai; /* Array index. */
	uint16_t ca; /* Current array index. */
	uint16_t na; /* Next array index. */
	uint16_t cq_ci; /* The next CQE. */
	uint32_t cqe_cnt; /* Number of CQEs. */
};

/* RX queue descriptor. */
struct rxq {
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int vlan_strip:1; /* Enable VLAN stripping. */
	unsigned int crc_present:1; /* CRC must be subtracted. */
	unsigned int sges_n:2; /* Log 2 of SGEs (max buffers per packet). */
	unsigned int cqe_n:4; /* Log 2 of CQ elements. */
	unsigned int elts_n:4; /* Log 2 of Mbufs. */
	unsigned int port_id:8;
	unsigned int rss_hash:1; /* RSS hash result is enabled. */
	unsigned int mark:1; /* Marked flow available on the queue. */
	unsigned int :8; /* Remaining bits. */
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
	uint16_t rq_ci;
	uint16_t cq_ci;
	volatile struct mlx5_wqe_data_seg(*wqes)[];
	volatile struct mlx5_cqe(*cqes)[];
	struct rxq_zip zip; /* Compressed context. */
	struct rte_mbuf *(*elts)[];
	struct rte_mempool *mp;
	struct mlx5_rxq_stats stats;
} __rte_cache_aligned;

/* RX queue control descriptor. */
struct rxq_ctrl {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_exp_wq *wq; /* Work Queue. */
	struct fdir_queue *fdir_queue; /* Flow director queue. */
	struct ibv_mr *mr; /* Memory Region (for mp). */
	struct ibv_comp_channel *channel;
	unsigned int socket; /* CPU socket ID for allocations. */
	struct rxq rxq; /* Data path structure. */
};

/* Hash RX queue types. */
enum hash_rxq_type {
	HASH_RXQ_TCPV4,
	HASH_RXQ_UDPV4,
	HASH_RXQ_IPV4,
	HASH_RXQ_TCPV6,
	HASH_RXQ_UDPV6,
	HASH_RXQ_IPV6,
	HASH_RXQ_ETH,
};

/* Flow structure with Ethernet specification. It is packed to prevent padding
 * between attr and spec as this layout is expected by libibverbs. */
struct flow_attr_spec_eth {
	struct ibv_exp_flow_attr attr;
	struct ibv_exp_flow_spec_eth spec;
} __attribute__((packed));

/* Define a struct flow_attr_spec_eth object as an array of at least
 * "size" bytes. Room after the first index is normally used to store
 * extra flow specifications. */
#define FLOW_ATTR_SPEC_ETH(name, size) \
	struct flow_attr_spec_eth name \
		[((size) / sizeof(struct flow_attr_spec_eth)) + \
		 !!((size) % sizeof(struct flow_attr_spec_eth))]

/* Initialization data for hash RX queue. */
struct hash_rxq_init {
	uint64_t hash_fields; /* Fields that participate in the hash. */
	uint64_t dpdk_rss_hf; /* Matching DPDK RSS hash fields. */
	unsigned int flow_priority; /* Flow priority to use. */
	union {
		struct {
			enum ibv_exp_flow_spec_type type;
			uint16_t size;
		} hdr;
		struct ibv_exp_flow_spec_tcp_udp tcp_udp;
		struct ibv_exp_flow_spec_ipv4 ipv4;
		struct ibv_exp_flow_spec_ipv6 ipv6;
		struct ibv_exp_flow_spec_eth eth;
	} flow_spec; /* Flow specification template. */
	const struct hash_rxq_init *underlayer; /* Pointer to underlayer. */
};

/* Initialization data for indirection table. */
struct ind_table_init {
	unsigned int max_size; /* Maximum number of WQs. */
	/* Hash RX queues using this table. */
	unsigned int hash_types;
	unsigned int hash_types_n;
};

/* Initialization data for special flows. */
struct special_flow_init {
	uint8_t dst_mac_val[6];
	uint8_t dst_mac_mask[6];
	unsigned int hash_types;
	unsigned int per_vlan:1;
};

enum hash_rxq_flow_type {
	HASH_RXQ_FLOW_TYPE_PROMISC,
	HASH_RXQ_FLOW_TYPE_ALLMULTI,
	HASH_RXQ_FLOW_TYPE_BROADCAST,
	HASH_RXQ_FLOW_TYPE_IPV6MULTI,
	HASH_RXQ_FLOW_TYPE_MAC,
};

#ifndef NDEBUG
static inline const char *
hash_rxq_flow_type_str(enum hash_rxq_flow_type flow_type)
{
	switch (flow_type) {
	case HASH_RXQ_FLOW_TYPE_PROMISC:
		return "promiscuous";
	case HASH_RXQ_FLOW_TYPE_ALLMULTI:
		return "allmulticast";
	case HASH_RXQ_FLOW_TYPE_BROADCAST:
		return "broadcast";
	case HASH_RXQ_FLOW_TYPE_IPV6MULTI:
		return "IPv6 multicast";
	case HASH_RXQ_FLOW_TYPE_MAC:
		return "MAC";
	}
	return NULL;
}
#endif /* NDEBUG */

struct hash_rxq {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_qp *qp; /* Hash RX QP. */
	enum hash_rxq_type type; /* Hash RX queue type. */
	/* MAC flow steering rules, one per VLAN ID. */
	struct ibv_exp_flow *mac_flow
		[MLX5_MAX_MAC_ADDRESSES][MLX5_MAX_VLAN_IDS];
	struct ibv_exp_flow *special_flow
		[MLX5_MAX_SPECIAL_FLOWS][MLX5_MAX_VLAN_IDS];
};

/* TX queue descriptor. */
RTE_STD_C11
struct txq {
	uint16_t elts_head; /* Current index in (*elts)[]. */
	uint16_t elts_tail; /* First element awaiting completion. */
	uint16_t elts_comp; /* Counter since last completion request. */
	uint16_t mpw_comp; /* WQ index since last completion request. */
	uint16_t cq_ci; /* Consumer index for completion queue. */
	uint16_t cq_pi; /* Producer index for completion queue. */
	uint16_t wqe_ci; /* Consumer index for work queue. */
	uint16_t wqe_pi; /* Producer index for work queue. */
	uint16_t elts_n:4; /* (*elts)[] length (in log2). */
	uint16_t cqe_n:4; /* Number of CQ elements (in log2). */
	uint16_t wqe_n:4; /* Number of of WQ elements (in log2). */
	uint16_t inline_en:1; /* When set inline is enabled. */
	uint16_t tso_en:1; /* When set hardware TSO is enabled. */
	uint16_t tunnel_en:1;
	/* When set TX offload for tunneled packets are supported. */
	uint16_t mpw_hdr_dseg:1; /* Enable DSEGs in the title WQEBB. */
	uint16_t max_inline; /* Multiple of RTE_CACHE_LINE_SIZE to inline. */
	uint16_t inline_max_packet_sz; /* Max packet size for inlining. */
	uint32_t qp_num_8s; /* QP number shifted by 8. */
	volatile struct mlx5_cqe (*cqes)[]; /* Completion queue. */
	volatile void *wqes; /* Work queue (use volatile to write into). */
	volatile uint32_t *qp_db; /* Work queue doorbell. */
	volatile uint32_t *cq_db; /* Completion queue doorbell. */
	volatile void *bf_reg; /* Blueflame register. */
	struct {
		const struct rte_mempool *mp; /* Cached Memory Pool. */
		struct ibv_mr *mr; /* Memory Region (for mp). */
		uint32_t lkey; /* htonl(mr->lkey) */
	} mp2mr[MLX5_PMD_TX_MP_CACHE]; /* MP to MR translation table. */
	struct rte_mbuf *(*elts)[]; /* TX elements. */
	struct mlx5_txq_stats stats; /* TX queue counters. */
} __rte_cache_aligned;

/* TX queue control descriptor. */
struct txq_ctrl {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
	unsigned int socket; /* CPU socket ID for allocations. */
	struct txq txq; /* Data path structure. */
};

/* mlx5_rxq.c */

extern const struct hash_rxq_init hash_rxq_init[];
extern const unsigned int hash_rxq_init_n;

extern uint8_t rss_hash_default_key[];
extern const size_t rss_hash_default_key_len;

size_t priv_flow_attr(struct priv *, struct ibv_exp_flow_attr *,
		      size_t, enum hash_rxq_type);
int priv_create_hash_rxqs(struct priv *);
void priv_destroy_hash_rxqs(struct priv *);
int priv_allow_flow_type(struct priv *, enum hash_rxq_flow_type);
int priv_rehash_flows(struct priv *);
int priv_intr_efd_enable(struct priv *priv);
void priv_intr_efd_disable(struct priv *priv);
int priv_create_intr_vec(struct priv *priv);
void priv_destroy_intr_vec(struct priv *priv);
void rxq_cleanup(struct rxq_ctrl *);
int rxq_rehash(struct rte_eth_dev *, struct rxq_ctrl *);
int rxq_ctrl_setup(struct rte_eth_dev *, struct rxq_ctrl *, uint16_t,
		   unsigned int, const struct rte_eth_rxconf *,
		   struct rte_mempool *);
int mlx5_rx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_rxconf *, struct rte_mempool *);
void mlx5_rx_queue_release(void *);
uint16_t mlx5_rx_burst_secondary_setup(void *, struct rte_mbuf **, uint16_t);

/* mlx5_txq.c */

void txq_cleanup(struct txq_ctrl *);
int txq_ctrl_setup(struct rte_eth_dev *, struct txq_ctrl *, uint16_t,
		   unsigned int, const struct rte_eth_txconf *);
int mlx5_tx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_txconf *);
void mlx5_tx_queue_release(void *);
uint16_t mlx5_tx_burst_secondary_setup(void *, struct rte_mbuf **, uint16_t);

/* mlx5_rxtx.c */

uint16_t mlx5_tx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_mpw(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_mpw_inline(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_tx_burst_empw(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_rx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t removed_tx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t removed_rx_burst(void *, struct rte_mbuf **, uint16_t);
int mlx5_rx_descriptor_status(void *, uint16_t);
int mlx5_tx_descriptor_status(void *, uint16_t);
int mlx5_rx_intr_enable(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int mlx5_rx_intr_disable(struct rte_eth_dev *dev, uint16_t rx_queue_id);

/* mlx5_mr.c */

struct ibv_mr *mlx5_mp2mr(struct ibv_pd *, struct rte_mempool *);
void txq_mp2mr_iter(struct rte_mempool *, void *);
uint32_t txq_mp2mr_reg(struct txq *, struct rte_mempool *, unsigned int);

#endif /* RTE_PMD_MLX5_RXTX_H_ */
