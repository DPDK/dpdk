/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_RXTX_H
#define ZXDH_RXTX_H

#include <stdint.h>

#include <rte_common.h>
#include <rte_mbuf_core.h>

struct zxdh_virtnet_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
	uint64_t multicast;
	uint64_t broadcast;
	uint64_t truncated_err;
	uint64_t size_bins[8];
};

struct zxdh_virtnet_rx {
	struct zxdh_virtqueue         *vq;

	/* dummy mbuf, for wraparound when processing RX ring. */
	struct rte_mbuf           fake_mbuf;

	uint64_t                  mbuf_initializer; /* value to init mbufs. */
	struct rte_mempool       *mpool;            /* mempool for mbuf allocation */
	uint16_t                  queue_id;         /* DPDK queue index. */
	uint16_t                  port_id;          /* Device port identifier. */
	struct zxdh_virtnet_stats      stats;
	const struct rte_memzone *mz;               /* mem zone to populate RX ring. */
} __rte_packed;

struct zxdh_virtnet_tx {
	struct zxdh_virtqueue         *vq;
	const struct rte_memzone *zxdh_net_hdr_mz;  /* memzone to populate hdr. */
	rte_iova_t                zxdh_net_hdr_mem; /* hdr for each xmit packet */
	uint16_t                  queue_id;           /* DPDK queue index. */
	uint16_t                  port_id;            /* Device port identifier. */
	struct zxdh_virtnet_stats      stats;
	const struct rte_memzone *mz;                 /* mem zone to populate TX ring. */
} __rte_packed;

#endif  /* ZXDH_RXTX_H */
