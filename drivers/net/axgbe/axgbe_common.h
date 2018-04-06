/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#ifndef __AXGBE_COMMON_H__
#define __AXGBE_COMMON_H__

#include "axgbe_logs.h"

#include <stdbool.h>
#include <limits.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

#include <rte_byteorder.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_hexdump.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_branch_prediction.h>
#include <rte_eal.h>
#include <rte_memzone.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_dev.h>
#include <rte_ethdev_pci.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_io.h>

#define BIT(nr)	                       (1 << (nr))
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define AXGBE_HZ				250

#endif /* __AXGBE_COMMON_H__ */
