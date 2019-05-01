/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_DEFS_H_
#define RTE_PMD_MLX5_DEFS_H_

#include <rte_ethdev_driver.h>

#include "mlx5_autoconf.h"

/* Reported driver name. */
#define MLX5_DRIVER_NAME "net_mlx5"

/* Maximum number of simultaneous unicast MAC addresses. */
#define MLX5_MAX_UC_MAC_ADDRESSES 128
/* Maximum number of simultaneous Multicast MAC addresses. */
#define MLX5_MAX_MC_MAC_ADDRESSES 128
/* Maximum number of simultaneous MAC addresses. */
#define MLX5_MAX_MAC_ADDRESSES \
	(MLX5_MAX_UC_MAC_ADDRESSES + MLX5_MAX_MC_MAC_ADDRESSES)

/* Maximum number of simultaneous VLAN filters. */
#define MLX5_MAX_VLAN_IDS 128

/*
 * Request TX completion every time descriptors reach this threshold since
 * the previous request. Must be a power of two for performance reasons.
 */
#define MLX5_TX_COMP_THRESH 32

/*
 * Request TX completion every time the total number of WQEBBs used for inlining
 * packets exceeds the size of WQ divided by this divisor. Better to be power of
 * two for performance.
 */
#define MLX5_TX_COMP_THRESH_INLINE_DIV (1 << 3)

/* Size of per-queue MR cache array for linear search. */
#define MLX5_MR_CACHE_N 8

/* Size of MR cache table for binary search. */
#define MLX5_MR_BTREE_CACHE_N 256

/*
 * If defined, only use software counters. The PMD will never ask the hardware
 * for these, and many of them won't be available.
 */
#ifndef MLX5_PMD_SOFT_COUNTERS
#define MLX5_PMD_SOFT_COUNTERS 1
#endif

/* Alarm timeout. */
#define MLX5_ALARM_TIMEOUT_US 100000

/* Maximum number of extended statistics counters. */
#define MLX5_MAX_XSTATS 32

/* Maximum Packet headers size (L2+L3+L4) for TSO. */
#define MLX5_MAX_TSO_HEADER 192

/* Default maximum number of Tx queues for vectorized Tx. */
#if defined(RTE_ARCH_ARM64)
#define MLX5_VPMD_MAX_TXQS 8
#define MLX5_VPMD_MAX_TXQS_BLUEFIELD 16
#else
#define MLX5_VPMD_MAX_TXQS 4
#define MLX5_VPMD_MAX_TXQS_BLUEFIELD MLX5_VPMD_MAX_TXQS
#endif

/* Threshold of buffer replenishment for vectorized Rx. */
#define MLX5_VPMD_RXQ_RPLNSH_THRESH(n) \
	(RTE_MIN(MLX5_VPMD_RX_MAX_BURST, (unsigned int)(n) >> 2))

/* Maximum size of burst for vectorized Rx. */
#define MLX5_VPMD_RX_MAX_BURST 64U

/*
 * Maximum size of burst for vectorized Tx. This is related to the maximum size
 * of Enhanced MPW (eMPW) WQE as vectorized Tx is supported with eMPW.
 * Careful when changing, large value can cause WQE DS to overlap.
 */
#define MLX5_VPMD_TX_MAX_BURST        32U

/* Number of packets vectorized Rx can simultaneously process in a loop. */
#define MLX5_VPMD_DESCS_PER_LOOP      4

/* Supported RSS */
#define MLX5_RSS_HF_MASK (~(ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP))

/* Timeout in seconds to get a valid link status. */
#define MLX5_LINK_STATUS_TIMEOUT 10

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
#define MLX5_MPRQ_STRIDE_NUM_N 6U

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

/* Definition of static_assert found in /usr/include/assert.h */
#ifndef HAVE_STATIC_ASSERT
#define static_assert _Static_assert
#endif

#endif /* RTE_PMD_MLX5_DEFS_H_ */
