/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_INCLUDE_H_
#define _NBL_INCLUDE_H_

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/genetlink.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <bus_pci_driver.h>
#include <rte_io.h>
#include <rte_tailq.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_thread.h>
#include <rte_spinlock.h>
#include <rte_stdatomic.h>
#include <rte_bitmap.h>
#include <rte_eal_paging.h>
#include <eal_interrupts.h>

#include "nbl_logs.h"

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

/* Used for macros to pass checkpatch */
#define NBL_NAME(x)				x
#define BIT(a)					(1 << (a))
#define NBL_SAFE_THREADS_WAIT_TIME		(20)

enum {
	NBL_VSI_DATA = 0,	/* default vsi in kernel or independent dpdk */
	NBL_VSI_CTRL,
	NBL_VSI_USER,		/* dpdk used vsi in coexist dpdk */
	NBL_VSI_MAX,
};

enum nbl_product_type {
	NBL_LEONIS_TYPE,
	NBL_PRODUCT_MAX,
};

struct nbl_func_caps {
	enum nbl_product_type product_type;
	u32 is_vf:1;
	u32 rsv:31;
};

struct nbl_start_rx_ring_param {
	u16 queue_idx;
	u16 nb_desc;
	u32 socket_id;
	enum nbl_product_type product;
	const struct rte_eth_rxconf *conf;
	struct rte_mempool *mempool;
};

struct nbl_start_tx_ring_param {
	u16 queue_idx;
	u16 nb_desc;
	u32 socket_id;
	enum nbl_product_type product;
	const struct rte_eth_txconf *conf;
	bool (*bond_broadcast_check)(struct rte_mbuf *mbuf);
};

struct nbl_txrx_queue_param {
	u16 vsi_id;
	u64 dma;
	u64 avail;
	u64 used;
	u16 desc_num;
	u16 local_queue_id;
	u16 intr_en;
	u16 intr_mask;
	u16 global_vector_id;
	u16 half_offload_en;
	u16 split;
	u16 extend_header;
	u16 cxt;
	u16 rxcsum;
};

struct nbl_board_port_info {
	u8 eth_num;
	u8 speed;
	u8 rsv[6];
};

struct nbl_common_info {
	const struct rte_eth_dev *eth_dev;
	u16 vsi_id;
	u16 instance_id;
	int devfd;
	int eventfd;
	int ifindex;
	int iommu_group_num;
	int nl_socket_route;
	int dma_limit_msb;
	u8 eth_id;
	/* isolate 1 means kernel network, 0 means user network */
	u8 isolate:1;
	/* curr_network 0 means kernel network, 1 means user network */
	u8 curr_network:1;
	u8 is_vf:1;
	u8 pf_start:1;
	u8 specific_dma:1;
	u8 dma_set_msb:1;
	u8 rsv:2;
	struct nbl_board_port_info board_info;
};

struct nbl_register_net_param {
	u16 pf_bdf;
	u64 vf_bar_start;
	u64 vf_bar_size;
	u16 total_vfs;
	u16 offset;
	u16 stride;
	u64 pf_bar_start;
};

struct nbl_register_net_result {
	u16 tx_queue_num;
	u16 rx_queue_num;
	u16 queue_size;
	u16 rdma_enable;
	u64 hw_features;
	u64 features;
	u16 max_mtu;
	u16 queue_offset;
	u8 mac[RTE_ETHER_ADDR_LEN];
	u16 vlan_proto;
	u16 vlan_tci;
	u32 rate;
	bool trusted;
};

struct nbl_eth_link_info {
	u8 link_status;
	u32 link_speed;
};

struct nbl_rxq_stats {
	uint64_t rx_packets;
	uint64_t rx_bytes;
	uint64_t rx_nombuf;
	uint64_t rx_multi_descs;

	uint64_t rx_ierror;
	uint64_t rx_drop_noport;
	uint64_t rx_drop_proto;
};

struct nbl_txq_stats {
	uint64_t tx_packets;
	uint64_t tx_bytes;
	uint64_t tx_errors;
	uint64_t tx_descs;
	uint64_t tx_tso_packets;
};

#endif
