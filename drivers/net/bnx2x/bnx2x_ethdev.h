/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * Copyright (c) 2015 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.bnx2x_pmd for copyright and licensing details.
 */

#ifndef PMD_BNX2X_ETHDEV_H
#define PMD_BNX2X_ETHDEV_H

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_spinlock.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "bnx2x_rxtx.h"
#include "bnx2x_logs.h"

#define DELAY(x) rte_delay_us(x)
#define DELAY_MS(x) rte_delay_ms(x)
#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000*(x))

#define FALSE               0
#define TRUE                1

#define false               0
#define true                1
#define min(a,b)        RTE_MIN(a,b)

#define mb()    rte_mb()
#define wmb()   rte_wmb()
#define rmb()   rte_rmb()


#define MAX_QUEUES sysconf(_SC_NPROCESSORS_CONF)

#define BNX2X_MIN_RX_BUF_SIZE 1024
#define BNX2X_MAX_RX_PKT_LEN  15872
#define BNX2X_MAX_MAC_ADDRS   1

/* Hardware RX tick timer (usecs) */
#define BNX2X_RX_TICKS 25
/* Hardware TX tick timer (usecs) */
#define BNX2X_TX_TICKS 50
/* Maximum number of Rx packets to process at a time */
#define BNX2X_RX_BUDGET 0xffffffff

#endif

/* MAC address operations */
struct bnx2x_mac_ops {
	void (*mac_addr_add)(struct rte_eth_dev *dev, struct ether_addr *addr,
			uint16_t index, uint32_t pool);                           /* not implemented yet */
	void (*mac_addr_remove)(struct rte_eth_dev *dev, uint16_t index); /* not implemented yet */
};
