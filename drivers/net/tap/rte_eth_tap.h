/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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

#ifndef _RTE_ETH_TAP_H_
#define _RTE_ETH_TAP_H_

#include <sys/queue.h>
#include <sys/uio.h>
#include <inttypes.h>

#include <linux/if_tun.h>

#include <rte_ethdev.h>
#include <rte_ether.h>

#ifdef IFF_MULTI_QUEUE
#define RTE_PMD_TAP_MAX_QUEUES	16
#else
#define RTE_PMD_TAP_MAX_QUEUES	1
#endif

struct pkt_stats {
	uint64_t opackets;              /* Number of output packets */
	uint64_t ipackets;              /* Number of input packets */
	uint64_t obytes;                /* Number of bytes on output */
	uint64_t ibytes;                /* Number of bytes on input */
	uint64_t errs;                  /* Number of TX error packets */
	uint64_t ierrors;               /* Number of RX error packets */
	uint64_t rx_nombuf;             /* Nb of RX mbuf alloc failures */
};

struct rx_queue {
	struct rte_mempool *mp;         /* Mempool for RX packets */
	uint32_t trigger_seen;          /* Last seen Rx trigger value */
	uint16_t in_port;               /* Port ID */
	int fd;
	struct pkt_stats stats;         /* Stats for this RX queue */
	uint16_t nb_rx_desc;            /* max number of mbufs available */
	struct rte_eth_rxmode *rxmode;  /* RX features */
	struct rte_mbuf *pool;          /* mbufs pool for this queue */
	struct iovec (*iovecs)[];       /* descriptors for this queue */
	struct tun_pi pi;               /* packet info for iovecs */
};

struct tx_queue {
	int fd;
	uint16_t *mtu;                  /* Pointer to MTU from dev_data */
	struct pkt_stats stats;         /* Stats for this TX queue */
};

struct pmd_internals {
	char remote_iface[RTE_ETH_NAME_MAX_LEN]; /* Remote netdevice name */
	char name[RTE_ETH_NAME_MAX_LEN];  /* Internal Tap device name */
	uint16_t nb_queues;               /* Number of queues supported */
	struct ether_addr eth_addr;       /* Mac address of the device port */
	int remote_if_index;              /* remote netdevice IF_INDEX */
	int if_index;                     /* IF_INDEX for the port */
	int ioctl_sock;                   /* socket for ioctl calls */
	int nlsk_fd;                      /* Netlink socket fd */
	int flower_support;               /* 1 if kernel supports, else 0 */
	int flower_vlan_support;          /* 1 if kernel supports, else 0 */
	LIST_HEAD(tap_flows, rte_flow) flows;        /* rte_flow rules */
	/* implicit rte_flow rules set when a remote device is active */
	LIST_HEAD(tap_implicit_flows, rte_flow) implicit_flows;
	struct rx_queue rxq[RTE_PMD_TAP_MAX_QUEUES]; /* List of RX queues */
	struct tx_queue txq[RTE_PMD_TAP_MAX_QUEUES]; /* List of TX queues */
	struct rte_intr_handle intr_handle;          /* LSC interrupt handle. */
};

#endif /* _RTE_ETH_TAP_H_ */
