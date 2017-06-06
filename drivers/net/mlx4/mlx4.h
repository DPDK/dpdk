/*-
 *   BSD LICENSE
 *
 *   Copyright 2012-2017 6WIND S.A.
 *   Copyright 2012-2017 Mellanox.
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

#ifndef RTE_PMD_MLX4_H_
#define RTE_PMD_MLX4_H_

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/*
 * Runtime logging through RTE_LOG() is enabled when not in debugging mode.
 * Intermediate LOG_*() macros add the required end-of-line characters.
 */
#ifndef NDEBUG
#define INFO(...) DEBUG(__VA_ARGS__)
#define WARN(...) DEBUG(__VA_ARGS__)
#define ERROR(...) DEBUG(__VA_ARGS__)
#else
#define LOG__(level, m, ...) \
	RTE_LOG(level, PMD, MLX4_DRIVER_NAME ": " m "%c", __VA_ARGS__)
#define LOG_(level, ...) LOG__(level, __VA_ARGS__, '\n')
#define INFO(...) LOG_(INFO, __VA_ARGS__)
#define WARN(...) LOG_(WARNING, __VA_ARGS__)
#define ERROR(...) LOG_(ERR, __VA_ARGS__)
#endif

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/*
 * Maximum number of simultaneous MAC addresses supported.
 *
 * According to ConnectX's Programmer Reference Manual:
 *   The L2 Address Match is implemented by comparing a MAC/VLAN combination
 *   of 128 MAC addresses and 127 VLAN values, comprising 128x127 possible
 *   L2 addresses.
 */
#define MLX4_MAX_MAC_ADDRESSES 128

/* Maximum number of simultaneous VLAN filters supported. See above. */
#define MLX4_MAX_VLAN_IDS 127

/* Request send completion once in every 64 sends, might be less. */
#define MLX4_PMD_TX_PER_COMP_REQ 64

/* Maximum number of physical ports. */
#define MLX4_PMD_MAX_PHYS_PORTS 2

/* Maximum number of Scatter/Gather Elements per Work Request. */
#ifndef MLX4_PMD_SGE_WR_N
#define MLX4_PMD_SGE_WR_N 4
#endif

/* Maximum size for inline data. */
#ifndef MLX4_PMD_MAX_INLINE
#define MLX4_PMD_MAX_INLINE 0
#endif

/*
 * Maximum number of cached Memory Pools (MPs) per TX queue. Each RTE MP
 * from which buffers are to be transmitted will have to be mapped by this
 * driver to their own Memory Region (MR). This is a slow operation.
 *
 * This value is always 1 for RX queues.
 */
#ifndef MLX4_PMD_TX_MP_CACHE
#define MLX4_PMD_TX_MP_CACHE 8
#endif

/*
 * If defined, only use software counters. The PMD will never ask the hardware
 * for these, and many of them won't be available.
 */
#ifndef MLX4_PMD_SOFT_COUNTERS
#define MLX4_PMD_SOFT_COUNTERS 1
#endif

/* Alarm timeout. */
#define MLX4_ALARM_TIMEOUT_US 100000

/* Port parameter. */
#define MLX4_PMD_PORT_KVARG "port"

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX3 = 0x1003,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3VF = 0x1004,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO = 0x1007,
};

#define MLX4_DRIVER_NAME "net_mlx4"

/* Bit-field manipulation. */
#define BITFIELD_DECLARE(bf, type, size)				\
	type bf[(((size_t)(size) / (sizeof(type) * CHAR_BIT)) +		\
		 !!((size_t)(size) % (sizeof(type) * CHAR_BIT)))]
#define BITFIELD_DEFINE(bf, type, size)					\
	BITFIELD_DECLARE((bf), type, (size)) = { 0 }
#define BITFIELD_SET(bf, b)						\
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)),			\
	 (void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] |=		\
		((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))
#define BITFIELD_RESET(bf, b)						\
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)),			\
	 (void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] &=		\
		~((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))
#define BITFIELD_ISSET(bf, b)						\
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)),			\
	 !!(((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] &		\
	     ((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT))))))

/* Number of elements in array. */
#define elemof(a) (sizeof(a) / sizeof((a)[0]))

/* Cast pointer p to structure member m to its parent structure of type t. */
#define containerof(p, t, m) ((t *)((uint8_t *)(p) - offsetof(t, m)))

/* Branch prediction helpers. */
#ifndef likely
#define likely(c) __builtin_expect(!!(c), 1)
#endif
#ifndef unlikely
#define unlikely(c) __builtin_expect(!!(c), 0)
#endif

/* Debugging */
#ifndef NDEBUG
#include <stdio.h>
#define DEBUG__(m, ...)						\
	(fprintf(stderr, "%s:%d: %s(): " m "%c",		\
		 __FILE__, __LINE__, __func__, __VA_ARGS__),	\
	 fflush(stderr),					\
	 (void)0)
/*
 * Save/restore errno around DEBUG__().
 * XXX somewhat undefined behavior, but works.
 */
#define DEBUG_(...)				\
	(errno = ((int []){			\
		*(volatile int *)&errno,	\
		(DEBUG__(__VA_ARGS__), 0)	\
	})[0])
#define DEBUG(...) DEBUG_(__VA_ARGS__, '\n')
#define claim_zero(...) assert((__VA_ARGS__) == 0)
#define claim_nonzero(...) assert((__VA_ARGS__) != 0)
#define claim_positive(...) assert((__VA_ARGS__) >= 0)
#else /* NDEBUG */
/* No-ops. */
#define DEBUG(...) (void)0
#define claim_zero(...) (__VA_ARGS__)
#define claim_nonzero(...) (__VA_ARGS__)
#define claim_positive(...) (__VA_ARGS__)
#endif /* NDEBUG */

struct mlx4_rxq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX4_PMD_SOFT_COUNTERS
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
#endif
	uint64_t idropped; /**< Total of packets dropped when RX ring full. */
	uint64_t rx_nombuf; /**< Total of RX mbuf allocation failures. */
};

/* RX element (scattered packets). */
struct rxq_elt_sp {
	struct ibv_recv_wr wr; /* Work Request. */
	struct ibv_sge sges[MLX4_PMD_SGE_WR_N]; /* Scatter/Gather Elements. */
	struct rte_mbuf *bufs[MLX4_PMD_SGE_WR_N]; /* SGEs buffers. */
};

/* RX element. */
struct rxq_elt {
	struct ibv_recv_wr wr; /* Work Request. */
	struct ibv_sge sge; /* Scatter/Gather Element. */
	/* mbuf pointer is derived from WR_ID(wr.wr_id).offset. */
};

/* RX queue descriptor. */
struct rxq {
	struct priv *priv; /* Back pointer to private data. */
	struct rte_mempool *mp; /* Memory Pool for allocations. */
	struct ibv_mr *mr; /* Memory Region (for mp). */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
	struct ibv_exp_qp_burst_family *if_qp; /* QP burst interface. */
	struct ibv_exp_cq_family *if_cq; /* CQ interface. */
	struct ibv_comp_channel *channel;
	/*
	 * Each VLAN ID requires a separate flow steering rule.
	 */
	BITFIELD_DECLARE(mac_configured, uint32_t, MLX4_MAX_MAC_ADDRESSES);
	struct ibv_flow *mac_flow[MLX4_MAX_MAC_ADDRESSES][MLX4_MAX_VLAN_IDS];
	struct ibv_flow *promisc_flow; /* Promiscuous flow. */
	struct ibv_flow *allmulti_flow; /* Multicast flow. */
	unsigned int port_id; /* Port ID for incoming packets. */
	unsigned int elts_n; /* (*elts)[] length. */
	unsigned int elts_head; /* Current index in (*elts)[]. */
	union {
		struct rxq_elt_sp (*sp)[]; /* Scattered RX elements. */
		struct rxq_elt (*no_sp)[]; /* RX elements. */
	} elts;
	unsigned int sp:1; /* Use scattered RX elements. */
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int csum_l2tun:1; /* Same for L2 tunnels. */
	struct mlx4_rxq_stats stats; /* RX queue counters. */
	unsigned int socket; /* CPU socket ID for allocations. */
	struct ibv_exp_res_domain *rd; /* Resource Domain. */
};

/* TX element. */
struct txq_elt {
	struct rte_mbuf *buf;
};

struct mlx4_txq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX4_PMD_SOFT_COUNTERS
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes;   /**< Total of successfully sent bytes. */
#endif
	uint64_t odropped; /**< Total of packets not sent when TX ring full. */
};

/*
 * Linear buffer type. It is used when transmitting buffers with too many
 * segments that do not fit the hardware queue (see max_send_sge).
 * Extra segments are copied (linearized) in such buffers, replacing the
 * last SGE during TX.
 * The size is arbitrary but large enough to hold a jumbo frame with
 * 8 segments considering mbuf.buf_len is about 2048 bytes.
 */
typedef uint8_t linear_t[16384];

/* TX queue descriptor. */
struct txq {
	struct priv *priv; /* Back pointer to private data. */
	struct {
		const struct rte_mempool *mp; /* Cached Memory Pool. */
		struct ibv_mr *mr; /* Memory Region (for mp). */
		uint32_t lkey; /* mr->lkey */
	} mp2mr[MLX4_PMD_TX_MP_CACHE]; /* MP to MR translation table. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
	struct ibv_exp_qp_burst_family *if_qp; /* QP burst interface. */
	struct ibv_exp_cq_family *if_cq; /* CQ interface. */
#if MLX4_PMD_MAX_INLINE > 0
	uint32_t max_inline; /* Max inline send size <= MLX4_PMD_MAX_INLINE. */
#endif
	unsigned int elts_n; /* (*elts)[] length. */
	struct txq_elt (*elts)[]; /* TX elements. */
	unsigned int elts_head; /* Current index in (*elts)[]. */
	unsigned int elts_tail; /* First element awaiting completion. */
	unsigned int elts_comp; /* Number of completion requests. */
	unsigned int elts_comp_cd; /* Countdown for next completion request. */
	unsigned int elts_comp_cd_init; /* Initial value for countdown. */
	struct mlx4_txq_stats stats; /* TX queue counters. */
	linear_t (*elts_linear)[]; /* Linearized buffers. */
	struct ibv_mr *mr_linear; /* Memory Region for linearized buffers. */
	unsigned int socket; /* CPU socket ID for allocations. */
	struct ibv_exp_res_domain *rd; /* Resource Domain. */
};

struct rte_flow;

struct priv {
	struct rte_eth_dev *dev; /* Ethernet device. */
	struct ibv_context *ctx; /* Verbs context. */
	struct ibv_device_attr device_attr; /* Device properties. */
	struct ibv_pd *pd; /* Protection Domain. */
	/*
	 * MAC addresses array and configuration bit-field.
	 * An extra entry that cannot be modified by the DPDK is reserved
	 * for broadcast frames (destination MAC address ff:ff:ff:ff:ff:ff).
	 */
	struct ether_addr mac[MLX4_MAX_MAC_ADDRESSES];
	BITFIELD_DECLARE(mac_configured, uint32_t, MLX4_MAX_MAC_ADDRESSES);
	/* VLAN filters. */
	struct {
		unsigned int enabled:1; /* If enabled. */
		unsigned int id:12; /* VLAN ID (0-4095). */
	} vlan_filter[MLX4_MAX_VLAN_IDS]; /* VLAN filters table. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	uint8_t port; /* Physical port number. */
	unsigned int started:1; /* Device started, flows enabled. */
	unsigned int promisc:1; /* Device in promiscuous mode. */
	unsigned int allmulti:1; /* Device receives all multicast packets. */
	unsigned int hw_qpg:1; /* QP groups are supported. */
	unsigned int hw_tss:1; /* TSS is supported. */
	unsigned int hw_rss:1; /* RSS is supported. */
	unsigned int hw_csum:1; /* Checksum offload is supported. */
	unsigned int hw_csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int rss:1; /* RSS is enabled. */
	unsigned int vf:1; /* This is a VF device. */
	unsigned int pending_alarm:1; /* An alarm is pending. */
#ifdef INLINE_RECV
	unsigned int inl_recv_size; /* Inline recv size */
#endif
	unsigned int max_rss_tbl_sz; /* Maximum number of RSS queues. */
	/* RX/TX queues. */
	struct rxq rxq_parent; /* Parent queue when RSS is enabled. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct rxq *(*rxqs)[]; /* RX queues. */
	struct txq *(*txqs)[]; /* TX queues. */
	struct rte_intr_handle intr_handle; /* Interrupt handler. */
	struct rte_flow_drop *flow_drop_queue; /* Flow drop queue. */
	LIST_HEAD(mlx4_flows, rte_flow) flows;
	struct rte_intr_conf intr_conf; /* Active interrupt configuration. */
	rte_spinlock_t lock; /* Lock for control functions. */
};

void priv_lock(struct priv *priv);
void priv_unlock(struct priv *priv);

#endif /* RTE_PMD_MLX4_H_ */
