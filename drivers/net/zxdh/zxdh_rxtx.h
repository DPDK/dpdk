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

struct __rte_cache_aligned zxdh_virtnet_rx {
	struct zxdh_virtqueue         *vq;

	uint64_t                  mbuf_initializer; /* value to init mbufs. */
	struct rte_mempool       *mpool;            /* mempool for mbuf allocation */
	uint16_t                  queue_id;         /* DPDK queue index. */
	uint16_t                  port_id;          /* Device port identifier. */
	struct zxdh_virtnet_stats      stats;
	const struct rte_memzone *mz;               /* mem zone to populate RX ring. */

	/* dummy mbuf, for wraparound when processing RX ring. */
	struct rte_mbuf           fake_mbuf;
};

struct __rte_cache_aligned zxdh_virtnet_tx {
	struct zxdh_virtqueue         *vq;

	rte_iova_t                zxdh_net_hdr_mem; /* hdr for each xmit packet */
	uint16_t                  queue_id;           /* DPDK queue index. */
	uint16_t                  port_id;            /* Device port identifier. */
	struct zxdh_virtnet_stats      stats;
	const struct rte_memzone *mz;                 /* mem zone to populate TX ring. */
	const struct rte_memzone *zxdh_net_hdr_mz;  /* memzone to populate hdr. */
};

uint16_t zxdh_xmit_pkts_packed(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t zxdh_xmit_pkts_prepare(void *tx_queue __rte_unused, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts);
uint16_t zxdh_recv_pkts_packed(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts);

#endif  /* ZXDH_RXTX_H */
