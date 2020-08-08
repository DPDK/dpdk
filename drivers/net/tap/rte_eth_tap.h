/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _RTE_ETH_TAP_H_
#define _RTE_ETH_TAP_H_

#include <sys/queue.h>
#include <sys/uio.h>
#include <inttypes.h>
#include <net/if.h>

#include <linux/if_tun.h>

#include <rte_ethdev_driver.h>
#include <rte_ether.h>
#include <rte_gso.h>
#include "tap_log.h"

#ifdef IFF_MULTI_QUEUE
#define RTE_PMD_TAP_MAX_QUEUES	TAP_MAX_QUEUES
#else
#define RTE_PMD_TAP_MAX_QUEUES	1
#endif
#define MAX_GSO_MBUFS 64

enum rte_tuntap_type {
	ETH_TUNTAP_TYPE_UNKNOWN,
	ETH_TUNTAP_TYPE_TUN,
	ETH_TUNTAP_TYPE_TAP,
	ETH_TUNTAP_TYPE_MAX,
};

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
	uint16_t queue_id;		/* queue ID*/
	struct pkt_stats stats;         /* Stats for this RX queue */
	uint16_t nb_rx_desc;            /* max number of mbufs available */
	struct rte_eth_rxmode *rxmode;  /* RX features */
	struct rte_mbuf *pool;          /* mbufs pool for this queue */
	struct iovec (*iovecs)[];       /* descriptors for this queue */
	struct tun_pi pi;               /* packet info for iovecs */
};

struct tx_queue {
	int type;                       /* Type field - TUN|TAP */
	uint16_t *mtu;                  /* Pointer to MTU from dev_data */
	uint16_t csum:1;                /* Enable checksum offloading */
	struct pkt_stats stats;         /* Stats for this TX queue */
	struct rte_gso_ctx gso_ctx;     /* GSO context */
	uint16_t out_port;              /* Port ID */
	uint16_t queue_id;		/* queue ID*/
};

struct pmd_internals {
	struct rte_eth_dev *dev;          /* Ethernet device. */
	char remote_iface[RTE_ETH_NAME_MAX_LEN]; /* Remote netdevice name */
	char name[RTE_ETH_NAME_MAX_LEN];  /* Internal Tap device name */
	int type;                         /* Type field - TUN|TAP */
	struct rte_ether_addr eth_addr;   /* Mac address of the device port */
	struct ifreq remote_initial_flags;/* Remote netdevice flags on init */
	int remote_if_index;              /* remote netdevice IF_INDEX */
	int if_index;                     /* IF_INDEX for the port */
	int ioctl_sock;                   /* socket for ioctl calls */
	int nlsk_fd;                      /* Netlink socket fd */
	int flow_isolate;                 /* 1 if flow isolation is enabled */
	int flower_support;               /* 1 if kernel supports, else 0 */
	int flower_vlan_support;          /* 1 if kernel supports, else 0 */
	int rss_enabled;                  /* 1 if RSS is enabled, else 0 */
	/* implicit rules set when RSS is enabled */
	int map_fd;                       /* BPF RSS map fd */
	int bpf_fd[RTE_PMD_TAP_MAX_QUEUES];/* List of bpf fds per queue */
	LIST_HEAD(tap_rss_flows, rte_flow) rss_flows;
	LIST_HEAD(tap_flows, rte_flow) flows;        /* rte_flow rules */
	/* implicit rte_flow rules set when a remote device is active */
	LIST_HEAD(tap_implicit_flows, rte_flow) implicit_flows;
	struct rx_queue rxq[RTE_PMD_TAP_MAX_QUEUES]; /* List of RX queues */
	struct tx_queue txq[RTE_PMD_TAP_MAX_QUEUES]; /* List of TX queues */
	struct rte_intr_handle intr_handle;          /* LSC interrupt handle. */
	int ka_fd;                        /* keep-alive file descriptor */
	struct rte_mempool *gso_ctx_mp;     /* Mempool for GSO packets */
};

struct pmd_process_private {
	int rxq_fds[RTE_PMD_TAP_MAX_QUEUES];
	int txq_fds[RTE_PMD_TAP_MAX_QUEUES];
};

/* tap_intr.c */

int tap_rx_intr_vec_set(struct rte_eth_dev *dev, int set);

#endif /* _RTE_ETH_TAP_H_ */
