/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_TXRX_VEC_H
#define SXE2_TXRX_VEC_H

#include <ethdev_driver.h>
#include "sxe2_queue.h"

#define SXE2_RX_MODE_VEC_SIMPLE    RTE_BIT32(0)
#define SXE2_RX_MODE_VEC_OFFLOAD   RTE_BIT32(1)
#define SXE2_RX_MODE_VEC_SSE       RTE_BIT32(2)
#define SXE2_RX_MODE_BATCH_ALLOC   RTE_BIT32(10)
#define SXE2_RX_MODE_VEC_SET_MASK	(SXE2_RX_MODE_VEC_SIMPLE | \
			SXE2_RX_MODE_VEC_OFFLOAD | SXE2_RX_MODE_VEC_SSE)
#define SXE2_TX_MODE_VEC_SIMPLE   RTE_BIT32(0)
#define SXE2_TX_MODE_VEC_OFFLOAD  RTE_BIT32(1)
#define SXE2_TX_MODE_VEC_SSE      RTE_BIT32(2)
#define SXE2_TX_MODE_SIMPLE_BATCH RTE_BIT32(10)
#define SXE2_TX_MODE_VEC_SET_MASK	(SXE2_TX_MODE_VEC_SIMPLE | \
			SXE2_TX_MODE_VEC_OFFLOAD | SXE2_TX_MODE_VEC_SSE)
#define SXE2_TX_VEC_NO_SUPPORT_OFFLOAD (		  \
			RTE_ETH_TX_OFFLOAD_MULTI_SEGS |		  \
			RTE_ETH_TX_OFFLOAD_QINQ_INSERT |	  \
			RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM | \
			RTE_ETH_TX_OFFLOAD_TCP_TSO |	      \
			RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |    \
			RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |      \
			RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |     \
			RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |   \
			RTE_ETH_TX_OFFLOAD_SECURITY |   \
			RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)
#define SXE2_TX_VEC_SUPPORT_OFFLOAD (		    \
			RTE_ETH_TX_OFFLOAD_VLAN_INSERT |	\
			RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |		\
			RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |		\
			RTE_ETH_TX_OFFLOAD_UDP_CKSUM |		\
			RTE_ETH_TX_OFFLOAD_TCP_CKSUM)
#define SXE2_RX_VEC_NO_SUPPORT_OFFLOAD (	    \
		RTE_ETH_RX_OFFLOAD_TIMESTAMP |      \
		RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT |    \
		RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM | \
		RTE_ETH_RX_OFFLOAD_SECURITY |        \
		RTE_ETH_RX_OFFLOAD_QINQ_STRIP)
#define SXE2_RX_VEC_SUPPORT_OFFLOAD (		\
		RTE_ETH_RX_OFFLOAD_CHECKSUM |		\
		RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |		\
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |	    \
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |	\
		RTE_ETH_RX_OFFLOAD_RSS_HASH)
#ifdef RTE_ARCH_X86
uint16_t sxe2_tx_pkts_vec_sse(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t sxe2_tx_pkts_vec_sse_simple(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t sxe2_rx_pkts_scattered_vec_sse_offload(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
#endif
int32_t __rte_cold sxe2_tx_vec_support_check(struct rte_eth_dev *dev, uint32_t *vec_flags);
int32_t __rte_cold sxe2_tx_queues_vec_prepare(struct rte_eth_dev *dev);
int32_t __rte_cold sxe2_rx_vec_support_check(struct rte_eth_dev *dev, uint32_t *vec_flags);
bool __rte_cold sxe2_rx_offload_en_check(struct rte_eth_dev *dev, uint64_t offload);
int32_t __rte_cold sxe2_rx_queues_vec_prepare(struct rte_eth_dev *dev);

#endif /* SXE2_TXRX_VEC_H */
