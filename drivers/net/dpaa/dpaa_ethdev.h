/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2014-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017 NXP.
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
 *     * Neither the name of  Freescale Semiconductor, Inc nor the names of its
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
#ifndef __DPAA_ETHDEV_H__
#define __DPAA_ETHDEV_H__

/* System headers */
#include <stdbool.h>
#include <rte_ethdev.h>

#include <fsl_usd.h>
#include <fsl_qman.h>
#include <fsl_bman.h>
#include <of.h>
#include <netcfg.h>

/* DPAA SoC identifier; If this is not available, it can be concluded
 * that board is non-DPAA. Single slot is currently supported.
 */
#define DPAA_SOC_ID_FILE		"sys/devices/soc0/soc_id"

#define DPAA_MBUF_HW_ANNOTATION		64
#define DPAA_FD_PTA_SIZE		64

#if (DPAA_MBUF_HW_ANNOTATION + DPAA_FD_PTA_SIZE) > RTE_PKTMBUF_HEADROOM
#error "Annotation requirement is more than RTE_PKTMBUF_HEADROOM"
#endif

/* we will re-use the HEADROOM for annotation in RX */
#define DPAA_HW_BUF_RESERVE	0
#define DPAA_PACKET_LAYOUT_ALIGN	64

/* Alignment to use for cpu-local structs to avoid coherency problems. */
#define MAX_CACHELINE			64

#define DPAA_MIN_RX_BUF_SIZE 512
#define DPAA_MAX_RX_PKT_LEN  10240

/* RX queue tail drop threshold
 * currently considering 32 KB packets.
 */
#define CONG_THRESHOLD_RX_Q  (32 * 1024)

/*max mac filter for memac(8) including primary mac addr*/
#define DPAA_MAX_MAC_FILTER (MEMAC_NUM_OF_PADDRS + 1)

/*Maximum number of slots available in TX ring*/
#define MAX_TX_RING_SLOTS	8

/* PCD frame queues */
#define DPAA_PCD_FQID_START		0x400
#define DPAA_PCD_FQID_MULTIPLIER	0x100
#define DPAA_DEFAULT_NUM_PCD_QUEUES	1

#define DPAA_IF_TX_PRIORITY		3
#define DPAA_IF_RX_PRIORITY		4
#define DPAA_IF_DEBUG_PRIORITY		7

#define DPAA_IF_RX_ANNOTATION_STASH	1
#define DPAA_IF_RX_DATA_STASH		1
#define DPAA_IF_RX_CONTEXT_STASH		0

/* Each "debug" FQ is represented by one of these */
#define DPAA_DEBUG_FQ_RX_ERROR   0
#define DPAA_DEBUG_FQ_TX_ERROR   1

#define DPAA_RSS_OFFLOAD_ALL ( \
	ETH_RSS_FRAG_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_NONFRAG_IPV4_SCTP | \
	ETH_RSS_FRAG_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_NONFRAG_IPV6_SCTP)

#define DPAA_TX_CKSUM_OFFLOAD_MASK (             \
		PKT_TX_IP_CKSUM |                \
		PKT_TX_TCP_CKSUM |               \
		PKT_TX_UDP_CKSUM)

/* DPAA Frame descriptor macros */

#define DPAA_FD_CMD_FCO			0x80000000
/**< Frame queue Context Override */
#define DPAA_FD_CMD_RPD			0x40000000
/**< Read Prepended Data */
#define DPAA_FD_CMD_UPD			0x20000000
/**< Update Prepended Data */
#define DPAA_FD_CMD_DTC			0x10000000
/**< Do IP/TCP/UDP Checksum */
#define DPAA_FD_CMD_DCL4C		0x10000000
/**< Didn't calculate L4 Checksum */
#define DPAA_FD_CMD_CFQ			0x00ffffff
/**< Confirmation Frame Queue */

/* Each network interface is represented by one of these */
struct dpaa_if {
	int valid;
	char *name;
	const struct fm_eth_port_cfg *cfg;
	struct qman_fq *rx_queues;
	struct qman_fq *tx_queues;
	struct qman_fq debug_queues[2];
	uint16_t nb_rx_queues;
	uint16_t nb_tx_queues;
	uint32_t ifid;
	struct fman_if *fif;
	struct dpaa_bp_info *bp_info;
	struct rte_eth_fc_conf *fc_conf;
};

struct dpaa_if_stats {
	/* Rx Statistics Counter */
	uint64_t reoct;		/**<Rx Eth Octets Counter */
	uint64_t roct;		/**<Rx Octet Counters */
	uint64_t raln;		/**<Rx Alignment Error Counter */
	uint64_t rxpf;		/**<Rx valid Pause Frame */
	uint64_t rfrm;		/**<Rx Frame counter */
	uint64_t rfcs;		/**<Rx frame check seq error */
	uint64_t rvlan;		/**<Rx Vlan Frame Counter */
	uint64_t rerr;		/**<Rx Frame error */
	uint64_t ruca;		/**<Rx Unicast */
	uint64_t rmca;		/**<Rx Multicast */
	uint64_t rbca;		/**<Rx Broadcast */
	uint64_t rdrp;		/**<Rx Dropped Packet */
	uint64_t rpkt;		/**<Rx packet */
	uint64_t rund;		/**<Rx undersized packets */
	uint32_t res_x[14];
	uint64_t rovr;		/**<Rx oversized but good */
	uint64_t rjbr;		/**<Rx oversized with bad csum */
	uint64_t rfrg;		/**<Rx fragment Packet */
	uint64_t rcnp;		/**<Rx control packets (0x8808 */
	uint64_t rdrntp;	/**<Rx dropped due to FIFO overflow */
	uint32_t res01d0[12];
	/* Tx Statistics Counter */
	uint64_t teoct;		/**<Tx eth octets */
	uint64_t toct;		/**<Tx Octets */
	uint32_t res0210[2];
	uint64_t txpf;		/**<Tx valid pause frame */
	uint64_t tfrm;		/**<Tx frame counter */
	uint64_t tfcs;		/**<Tx FCS error */
	uint64_t tvlan;		/**<Tx Vlan Frame */
	uint64_t terr;		/**<Tx frame error */
	uint64_t tuca;		/**<Tx Unicast */
	uint64_t tmca;		/**<Tx Multicast */
	uint64_t tbca;		/**<Tx Broadcast */
	uint32_t res0258[2];
	uint64_t tpkt;		/**<Tx Packet */
	uint64_t tund;		/**<Tx Undersized */
};

#endif
