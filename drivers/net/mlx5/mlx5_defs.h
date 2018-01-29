/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox.
 */

#ifndef RTE_PMD_MLX5_DEFS_H_
#define RTE_PMD_MLX5_DEFS_H_

#include <rte_ethdev_driver.h>

#include "mlx5_autoconf.h"

/* Reported driver name. */
#define MLX5_DRIVER_NAME "net_mlx5"

/* Maximum number of simultaneous MAC addresses. */
#define MLX5_MAX_MAC_ADDRESSES 128

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

/*
 * Maximum number of cached Memory Pools (MPs) per TX queue. Each RTE MP
 * from which buffers are to be transmitted will have to be mapped by this
 * driver to their own Memory Region (MR). This is a slow operation.
 *
 * This value is always 1 for RX queues.
 */
#ifndef MLX5_PMD_TX_MP_CACHE
#define MLX5_PMD_TX_MP_CACHE 8
#endif

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
#define MLX5_MAX_TSO_HEADER 128

/* Default minimum number of Tx queues for vectorized Tx. */
#define MLX5_VPMD_MIN_TXQS 4

/* Threshold of buffer replenishment for vectorized Rx. */
#define MLX5_VPMD_RXQ_RPLNSH_THRESH   64U

/* Maximum size of burst for vectorized Rx. */
#define MLX5_VPMD_RX_MAX_BURST        MLX5_VPMD_RXQ_RPLNSH_THRESH

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

/* Maximum number of attempts to query link status before giving up. */
#define MLX5_MAX_LINK_QUERY_ATTEMPTS 5

/* Reserved address space for UAR mapping. */
#define MLX5_UAR_SIZE (1ULL << 32)

/* Offset of reserved UAR address space to hugepage memory. Offset is used here
 * to minimize possibility of address next to hugepage being used by other code
 * in either primary or secondary process, failing to map TX UAR would make TX
 * packets invisible to HW.
 */
#define MLX5_UAR_OFFSET (1ULL << 32)

#endif /* RTE_PMD_MLX5_DEFS_H_ */
