/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN9K_ETHDEV_H__
#define __CN9K_ETHDEV_H__

#include <cnxk_ethdev.h>

struct cn9k_eth_txq {
	uint64_t cmd[8];
	int64_t fc_cache_pkts;
	uint64_t *fc_mem;
	void *lmt_addr;
	rte_iova_t io_addr;
	uint64_t lso_tun_fmt;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
} __plt_cache_aligned;

struct cn9k_eth_rxq {
	uint64_t mbuf_initializer;
	uint64_t data_off;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t rq;
	struct cnxk_timesync_info *tstamp;
} __plt_cache_aligned;

/* Rx and Tx routines */
void cn9k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn9k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

#endif /* __CN9K_ETHDEV_H__ */
