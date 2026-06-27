/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_MP_H
#define SXE2_MP_H

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_memzone.h>
#include <rte_stdatomic.h>

#define SXE2_MP_NAME		"sxe2_mp_msg"
#define SXE2_MP_MZ_NAME		"sxe2_stats_mz"

#define SXE2_MP_MSG_TIMEOUT 30

#define SXE2_MP_MAX_XSTATS	128

#define SXE2_MP_MAX_SPIN	100000

enum sxe2_mp_req_type {
	SXE2_MP_REQ_GET_STATS = 1,
	SXE2_MP_REQ_GET_XSTATS,
	SXE2_MP_REQ_RESET_STATS,
};

struct sxe2_mp_param {
	enum sxe2_mp_req_type type;
	uint32_t port_id;
	int result;
};

union sxe2_mp_shared_payload {
	struct {
		struct rte_eth_stats stats;
		struct eth_queue_stats qstats;
	} stats_blk;
	struct {
		struct rte_eth_xstat xstats[SXE2_MP_MAX_XSTATS];
		uint32_t xstats_num;
	} xstats_blk;
};

struct sxe2_mp_shared_data {
	RTE_ATOMIC(uint16_t)in_use;
	union sxe2_mp_shared_payload payload;
};

int sxe2_mp_init(struct rte_eth_dev *dev);

void sxe2_mp_uninit(struct rte_eth_dev *dev);

int sxe2_mp_request_simple(struct rte_eth_dev *dev,
			   enum sxe2_mp_req_type type,
			   int *result_out);

int sxe2_mp_req_get_stats(struct rte_eth_dev *dev,
			  struct rte_eth_stats *stats,
			  struct eth_queue_stats *qstats);

int sxe2_mp_req_get_xstats(struct rte_eth_dev *dev,
			   struct rte_eth_xstat *xstats, uint32_t usr_cnt);

int sxe2_mp_req_reset_stats(struct rte_eth_dev *dev);

#endif
