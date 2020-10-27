/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Atomic Rules LLC
 */

#ifndef RTE_PMD_ARK_H
#define RTE_PMD_ARK_H

/**
 * @file
 * ARK driver-specific API
 */

#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#ifndef RTE_PMD_ARK_TX_USERDATA_ENABLE
#define RTE_PMD_ARK_TX_USERDATA_ENABLE 0
#endif

#ifndef RTE_PMD_ARK_RX_USERDATA_ENABLE
#define RTE_PMD_ARK_RX_USERDATA_ENABLE 0
#endif

typedef uint32_t rte_pmd_ark_tx_userdata_t;
typedef uint64_t rte_pmd_ark_rx_userdata_t;

extern int rte_pmd_ark_tx_userdata_dynfield_offset;
extern int rte_pmd_ark_rx_userdata_dynfield_offset;

/** mbuf dynamic field for custom Tx ARK data */
#define RTE_PMD_ARK_TX_USERDATA_DYNFIELD_NAME "rte_net_ark_dynfield_tx_userdata"
/** mbuf dynamic field for custom Rx ARK data */
#define RTE_PMD_ARK_RX_USERDATA_DYNFIELD_NAME "rte_net_ark_dynfield_rx_userdata"

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Read Tx user data from mbuf.
 *
 * @param mbuf Structure to read from.
 * @return user data
 */
__rte_experimental
static inline rte_pmd_ark_tx_userdata_t
rte_pmd_ark_mbuf_tx_userdata_get(const struct rte_mbuf *mbuf)
{
#if RTE_PMD_ARK_TX_USERDATA_ENABLE
	return *RTE_MBUF_DYNFIELD(mbuf, rte_pmd_ark_tx_userdata_dynfield_offset,
				  rte_pmd_ark_tx_userdata_t *);
#else
	RTE_SET_USED(mbuf);
	return 0;
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Write Tx user data to mbuf.
 *
 * @param mbuf Structure to write into.
 * @param data User data.
 */
__rte_experimental
static inline void
rte_pmd_ark_mbuf_tx_userdata_set(struct rte_mbuf *mbuf,
		rte_pmd_ark_tx_userdata_t data)
{
#if RTE_PMD_ARK_TX_USERDATA_ENABLE
	*RTE_MBUF_DYNFIELD(mbuf, rte_pmd_ark_tx_userdata_dynfield_offset,
			rte_pmd_ark_tx_userdata_t *) = data;
#else
	RTE_SET_USED(mbuf);
	RTE_SET_USED(data);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Read Rx user data from mbuf.
 *
 * @param mbuf Structure to read from.
 * @return user data
 */
__rte_experimental
static inline rte_pmd_ark_rx_userdata_t
rte_pmd_ark_mbuf_rx_userdata_get(const struct rte_mbuf *mbuf)
{
#if RTE_PMD_ARK_RX_USERDATA_ENABLE
	return *RTE_MBUF_DYNFIELD(mbuf, rte_pmd_ark_rx_userdata_dynfield_offset,
			rte_pmd_ark_rx_userdata_t *);
#else
	RTE_SET_USED(mbuf);
	return 0;
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Write Rx user data to mbuf.
 *
 * @param mbuf Structure to write into.
 * @param data User data.
 */
__rte_experimental
static inline void
rte_pmd_ark_mbuf_rx_userdata_set(struct rte_mbuf *mbuf,
		rte_pmd_ark_rx_userdata_t data)
{
#if RTE_PMD_ARK_RX_USERDATA_ENABLE
	*RTE_MBUF_DYNFIELD(mbuf, rte_pmd_ark_rx_userdata_dynfield_offset,
			rte_pmd_ark_rx_userdata_t *) = data;
#else
	RTE_SET_USED(mbuf);
	RTE_SET_USED(data);
#endif
}

#endif /* RTE_PMD_ARK_H */
