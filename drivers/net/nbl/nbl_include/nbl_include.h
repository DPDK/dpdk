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
#define NBL_NAME(x)					x

enum nbl_product_type {
	NBL_LEONIS_TYPE,
	NBL_PRODUCT_MAX,
};

struct nbl_func_caps {
	enum nbl_product_type product_type;
	u32 is_vf:1;
	u32 rsv:31;
};

#define BIT(a)			(1 << (a))

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

#endif
