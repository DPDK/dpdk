/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_RXTX_H
#define ZXDH_RXTX_H

#include <stdint.h>

#include <rte_common.h>
#include <rte_mbuf_core.h>

#define ZXDH_PORT_NP     0
#define ZXDH_PORT_DRS    1
#define ZXDH_PORT_DTP    2

/*PI PKT FLAG */
#define ZXDH_PKT_FORM_CPU                           0x20
#define ZXDH_NO_IP_FRAGMENT                         0x2000
#define ZXDH_NO_IPID_UPDATE                         0x4000
#define ZXDH_TX_IP_CKSUM_CAL                        0x8000
#define ZXDH_RX_IP_CKSUM_VERIFY                     0x01
#define ZXDH_RX_PSEUDO_CKSUM_VALID                  0x02
#define ZXDH_TX_TCPUDP_CKSUM_CAL                    0x04
#define ZXDH_RX_TCPUDP_CKSUM_VERIFY                 0x08
#define ZXDH_NO_TCP_FRAGMENT                        0x10
#define ZXDH_PI_FLAG                                0x20
#define ZXDH_PI_TYPE                                0x40
#define ZXDH_VERSION1                               0x80
#define ZXDH_PI_TYPE_PI                             0x00
#define ZXDH_PI_TYPE_VIRTIO95                       0x40
#define ZXDH_PI_TYPE_VIRTIO11                       0xC0

struct zxdh_virtnet_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
	uint64_t idle;
	uint64_t truncated_err;
	uint64_t offload_cfg_err;
	uint64_t invalid_hdr_len_err;
	uint64_t no_segs_err;
};

struct __rte_cache_aligned zxdh_virtnet_rx {
	struct zxdh_virtqueue         *vq;
	struct rte_mempool       *mpool;            /* mempool for mbuf allocation */
	struct zxdh_virtnet_stats      stats;
	const struct rte_memzone *mz;               /* mem zone to populate RX ring. */
	uint16_t                  queue_id;         /* DPDK queue index. */
	uint16_t                  port_id;          /* Device port identifier. */
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
uint16_t zxdh_xmit_pkts_prepare(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t zxdh_recv_pkts_packed(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t zxdh_recv_single_pkts(void *rx_queue, struct rte_mbuf **rcv_pkts, uint16_t nb_pkts);

#endif  /* ZXDH_RXTX_H */
