/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_DEFS_H_
#define RTE_PMD_MLX5_DEFS_H_

#include <ethdev_driver.h>
#include <rte_vxlan.h>

#include <mlx5_common_defs.h>

#include "mlx5_autoconf.h"

/* Maximum number of simultaneous VLAN filters. */
#define MLX5_MAX_VLAN_IDS 128

/*
 * Request TX completion every time descriptors reach this threshold since
 * the previous request. Must be a power of two for performance reasons.
 */
#define MLX5_TX_COMP_THRESH 32u

/*
 * Request TX completion every time the total number of WQEBBs used for inlining
 * packets exceeds the size of WQ divided by this divisor. Better to be power of
 * two for performance.
 */
#define MLX5_TX_COMP_THRESH_INLINE_DIV (1 << 3)

/*
 * Maximal amount of normal completion CQEs
 * processed in one call of tx_burst() routine.
 */
#define MLX5_TX_COMP_MAX_CQE 2u

/*
 * If defined, only use software counters. The PMD will never ask the hardware
 * for these, and many of them won't be available.
 */
#ifndef MLX5_PMD_SOFT_COUNTERS
#define MLX5_PMD_SOFT_COUNTERS 1
#endif

/* Maximum number of DCS created per port. */
#define MLX5_HWS_CNT_DCS_NUM 4

/* Alarm timeout. */
#define MLX5_ALARM_TIMEOUT_US 100000

/* Maximum number of extended statistics counters. */
#define MLX5_MAX_XSTATS 64

/* Maximum Packet headers size (L2+L3+L4) for TSO. */
#define MLX5_MAX_TSO_HEADER 192U

/* Inline data size required by NICs. */
#define MLX5_INLINE_HSIZE_NONE 0
#define MLX5_INLINE_HSIZE_L2 (sizeof(struct rte_ether_hdr) + \
			      sizeof(struct rte_vlan_hdr))
#define MLX5_INLINE_HSIZE_L3 (MLX5_INLINE_HSIZE_L2 + \
			      sizeof(struct rte_ipv6_hdr))
#define MLX5_INLINE_HSIZE_L4 (MLX5_INLINE_HSIZE_L3 + \
			      sizeof(struct rte_tcp_hdr))
#define MLX5_INLINE_HSIZE_INNER_L2 (MLX5_INLINE_HSIZE_L3 + \
				    sizeof(struct rte_udp_hdr) + \
				    sizeof(struct rte_vxlan_hdr) + \
				    sizeof(struct rte_ether_hdr) + \
				    sizeof(struct rte_vlan_hdr))
#define MLX5_INLINE_HSIZE_INNER_L3 (MLX5_INLINE_HSIZE_INNER_L2 + \
				    sizeof(struct rte_ipv6_hdr))
#define MLX5_INLINE_HSIZE_INNER_L4 (MLX5_INLINE_HSIZE_INNER_L3 + \
				    sizeof(struct rte_tcp_hdr))

/* Threshold of buffer replenishment for vectorized Rx. */
#define MLX5_VPMD_RXQ_RPLNSH_THRESH(n) \
	(RTE_MIN(MLX5_VPMD_RX_MAX_BURST, (unsigned int)(n) >> 2))

/* Maximum size of burst for vectorized Rx. */
#define MLX5_VPMD_RX_MAX_BURST 64U

/* Recommended optimal burst size. */
#define MLX5_RX_DEFAULT_BURST 64U
#define MLX5_TX_DEFAULT_BURST 64U

/* Number of packets vectorized Rx can simultaneously process in a loop. */
#define MLX5_VPMD_DESCS_PER_LOOP      4

/* Mask of RSS on source only or destination only. */
#define MLX5_RSS_SRC_DST_ONLY (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY | \
			       RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY)

/* Supported RSS */
#define MLX5_RSS_HF_MASK (~(RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP | \
			    MLX5_RSS_SRC_DST_ONLY | RTE_ETH_RSS_ESP))

/* Timeout in seconds to get a valid link status. */
#define MLX5_LINK_STATUS_TIMEOUT 10

/* Number of times to retry retrieving the physical link information. */
#define MLX5_GET_LINK_STATUS_RETRY_COUNT 3

/* Maximum number of UAR pages used by a port,
 * These are the size and mask for an array of mutexes used to synchronize
 * the access to port's UARs on platforms that do not support 64 bit writes.
 * In such systems it is possible to issue the 64 bits DoorBells through two
 * consecutive writes, each write 32 bits. The access to a UAR page (which can
 * be accessible by all threads in the process) must be synchronized
 * (for example, using a semaphore). Such a synchronization is not required
 * when ringing DoorBells on different UAR pages.
 * A port with 512 Tx queues uses 8, 4kBytes, UAR pages which are shared
 * among the ports.
 */
#define MLX5_UAR_PAGE_NUM_MAX 64
#define MLX5_UAR_PAGE_NUM_MASK ((MLX5_UAR_PAGE_NUM_MAX) - 1)

/* Log 2 of the default number of strides per WQE for Multi-Packet RQ. */
#define MLX5_MPRQ_DEFAULT_LOG_STRIDE_NUM 6U

/* Log 2 of the default size of a stride per WQE for Multi-Packet RQ. */
#define MLX5_MPRQ_DEFAULT_LOG_STRIDE_SIZE 11U

/* Two-byte shift is disabled for Multi-Packet RQ. */
#define MLX5_MPRQ_TWO_BYTE_SHIFT 0

/*
 * Minimum size of packet to be memcpy'd instead of being attached as an
 * external buffer.
 */
#define MLX5_MPRQ_MEMCPY_DEFAULT_LEN 128

/* Minimum number Rx queues to enable Multi-Packet RQ. */
#define MLX5_MPRQ_MIN_RXQS 12

/* Cache size of mempool for Multi-Packet RQ. */
#define MLX5_MPRQ_MP_CACHE_SZ 32U

/* MLX5_DV_XMETA_EN supported values. */
#define MLX5_XMETA_MODE_LEGACY 0
#define MLX5_XMETA_MODE_META16 1
#define MLX5_XMETA_MODE_META32 2
/* Provide info on patrial hw miss. Implies MLX5_XMETA_MODE_META16 */
#define MLX5_XMETA_MODE_MISS_INFO 3
/* Only valid in HWS, 32bits extended META without MARK support in FDB. */
#define MLX5_XMETA_MODE_META32_HWS 4

/* Tx accurate scheduling on timestamps parameters. */
#define MLX5_TXPP_WAIT_INIT_TS 1000ul /* How long to wait timestamp. */
#define MLX5_TXPP_CLKQ_SIZE 1
#define MLX5_TXPP_REARM	((1UL << MLX5_WQ_INDEX_WIDTH) / 4)
#define MLX5_TXPP_REARM_SQ_SIZE (((1UL << MLX5_CQ_INDEX_WIDTH) / \
				  MLX5_TXPP_REARM) * 2)
#define MLX5_TXPP_REARM_CQ_SIZE (MLX5_TXPP_REARM_SQ_SIZE / 2)
/* The minimal size test packet to put into one WQE, padded by HW. */
#define MLX5_TXPP_TEST_PKT_SIZE (sizeof(struct rte_ether_hdr) +	\
				 sizeof(struct rte_ipv4_hdr))

/* Size of the simple hash table for metadata register table. */
#define MLX5_FLOW_MREG_HTABLE_SZ 64
#define MLX5_FLOW_MREG_HNAME "MARK_COPY_TABLE"
#define MLX5_DEFAULT_COPY_ID UINT32_MAX

/* Size of the simple hash table for header modify table. */
#define MLX5_FLOW_HDR_MODIFY_HTABLE_SZ (1 << 15)

/* Size of the simple hash table for encap decap table. */
#define MLX5_FLOW_ENCAP_DECAP_HTABLE_SZ (1 << 12)

/* Size of the hash table for tag table. */
#define MLX5_TAGS_HLIST_ARRAY_SIZE	(1 << 15)

/* Size fo the hash table for SFT table. */
#define MLX5_FLOW_SFT_HLIST_ARRAY_SIZE	4096

/* Hairpin TX/RX queue configuration parameters. */
#define MLX5_HAIRPIN_QUEUE_STRIDE 6
#define MLX5_HAIRPIN_JUMBO_LOG_SIZE (14 + 2)

/* Maximum number of indirect actions supported by rte_flow */
#define MLX5_MAX_INDIRECT_ACTIONS 3

/* Maximum number of external Rx queues supported by rte_flow */
#define MLX5_MAX_EXT_RX_QUEUES (UINT16_MAX - RTE_PMD_MLX5_EXTERNAL_RX_QUEUE_ID_MIN + 1)

/*
 * Linux definition of static_assert is found in /usr/include/assert.h.
 * Windows does not require a redefinition.
 */
#if !defined(HAVE_STATIC_ASSERT) && !defined(RTE_EXEC_ENV_WINDOWS)
#define static_assert _Static_assert
#endif

#define MLX5_CNT_SVC_CYCLE_TIME_DEFAULT 500

#endif /* RTE_PMD_MLX5_DEFS_H_ */
