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

#endif
