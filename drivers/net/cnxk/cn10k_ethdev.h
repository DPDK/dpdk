/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_ETHDEV_H__
#define __CN10K_ETHDEV_H__

#include <cnxk_ethdev.h>

struct cn10k_eth_txq {
	uint64_t send_hdr_w0;
	uint64_t sg_w0;
	int64_t fc_cache_pkts;
	uint64_t *fc_mem;
	uintptr_t lmt_base;
	rte_iova_t io_addr;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
	uint64_t cmd[4];
	uint64_t lso_tun_fmt;
} __plt_cache_aligned;

struct cn10k_eth_rxq {
	uint64_t mbuf_initializer;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t data_off;
	uint16_t rq;
	struct cnxk_timesync_info *tstamp;
} __plt_cache_aligned;

/* Rx and Tx routines */
void cn10k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn10k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

#endif /* __CN10K_ETHDEV_H__ */
