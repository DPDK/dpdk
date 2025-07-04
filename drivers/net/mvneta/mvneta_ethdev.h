/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#ifndef _MVNETA_ETHDEV_H_
#define _MVNETA_ETHDEV_H_

#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_log.h>

/*
 * container_of is defined by both DPDK and MUSDK,
 * we'll declare only one version.
 *
 * Note that it is not used in this PMD anyway.
 */
#ifdef container_of
#undef container_of
#endif

#include <drivers/mv_neta.h>
#include <drivers/mv_neta_ppio.h>

/** Packet offset inside RX buffer. */
#define MRVL_NETA_PKT_OFFS 64

/** Maximum number of rx/tx queues per port */
#define MRVL_NETA_RXQ_MAX 8
#define MRVL_NETA_TXQ_MAX 8

/** Minimum/maximum number of descriptors in tx queue */
#define MRVL_NETA_TXD_MIN 16
#define MRVL_NETA_TXD_MAX 2048

/** Tx queue descriptors alignment in B */
#define MRVL_NETA_TXD_ALIGN 32

/** Minimum/maximum number of descriptors in rx queue */
#define MRVL_NETA_RXD_MIN 16
#define MRVL_NETA_RXD_MAX 2048

/** Rx queue descriptors alignment in B */
#define MRVL_NETA_RXD_ALIGN 32

#define MRVL_NETA_ETH_HDRS_LEN	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + \
				RTE_VLAN_HLEN)

#define MRVL_NETA_HDRS_LEN		(MV_MH_SIZE + MRVL_NETA_ETH_HDRS_LEN)
#define MRVL_NETA_MTU_TO_MRU(mtu)	((mtu) + MRVL_NETA_HDRS_LEN)
#define MRVL_NETA_MRU_TO_MTU(mru)	((mru) - MRVL_NETA_HDRS_LEN)

/** Rx offloads capabilities */
#define MVNETA_RX_OFFLOADS (RTE_ETH_RX_OFFLOAD_CHECKSUM)

/** Tx offloads capabilities */
#define MVNETA_TX_OFFLOAD_CHECKSUM (RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | \
				    RTE_ETH_TX_OFFLOAD_UDP_CKSUM  | \
				    RTE_ETH_TX_OFFLOAD_TCP_CKSUM)
#define MVNETA_TX_OFFLOADS (MVNETA_TX_OFFLOAD_CHECKSUM | \
			    RTE_ETH_TX_OFFLOAD_MULTI_SEGS)

#define MVNETA_TX_PKT_OFFLOADS (RTE_MBUF_F_TX_IP_CKSUM | \
				RTE_MBUF_F_TX_TCP_CKSUM | \
				RTE_MBUF_F_TX_UDP_CKSUM)

struct mvneta_priv {
	/* Hot fields, used in fast path. */
	struct neta_ppio	*ppio;    /**< Port handler pointer */

	uint8_t pp_id;
	uint8_t ppio_id;	/* ppio port id */
	uint8_t uc_mc_flushed;
	uint8_t multiseg;

	struct neta_ppio_params ppio_params;

	uint64_t rate_max;
	struct rte_eth_stats prev_stats;
};

/** Current log type. */
extern int mvneta_logtype;
#define RTE_LOGTYPE_MVNETA mvneta_logtype

#define MVNETA_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, MVNETA, "%s(): ", __func__, __VA_ARGS__)

#endif /* _MVNETA_ETHDEV_H_ */
