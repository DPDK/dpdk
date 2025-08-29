/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_RX_H_
#define _XSC_RX_H_

#define XSC_RX_FREE_THRESH	32

struct xsc_rxq_stats {
	uint64_t rx_pkts;   /* Total number of rx packets */
	uint64_t rx_bytes;  /* Total number of rx bytes */
	uint64_t rx_errors; /* Total number of rx error packets */
	uint64_t rx_nombuf; /* Total number of rx mbuf alloc failed */
};

struct __rte_cache_aligned xsc_rxq_data {
	uint16_t idx; /*QP idx */
	uint16_t port_id;
	void *cq; /* CQ pointer */
	void *qp; /* QP pointer */
	uint32_t cqn; /* CQ serial number */
	uint32_t qpn; /* QP serial number */
	uint16_t wqe_s; /* Number of WQE */
	uint16_t wqe_m; /* Mask of WQE number */
	uint16_t cqe_s; /* Number of CQE */
	uint16_t cqe_m; /* Mask of CQE number */
	uint16_t wqe_n:4; /* Log 2 of WQE number */
	uint16_t sge_n:4; /* Log 2 of each WQE DS number */
	uint16_t cqe_n:4; /* Log 2 of CQE number */
	uint16_t rsv0:4;
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
	volatile uint32_t *cq_pi;
	uint32_t rq_ci;
	uint32_t rq_pi;
	uint16_t cq_ci;
	uint16_t rx_free_thresh;
	uint16_t nb_rx_hold;
	volatile void *wqes;
	union {
		volatile struct xsc_cqe(*cqes)[];
		volatile struct xsc_cqe_u64(*cqes_u64)[];
	};
	struct rte_mbuf *(*elts)[]; /* Record the mbuf of wqe addr */
	struct rte_mempool *mp;
	const struct rte_memzone *rq_pas;  /* Palist memory */
	uint32_t socket;
	struct xsc_ethdev_priv *priv;
	struct xsc_rxq_stats stats;
	/* attr */
	uint16_t csum:1;  /* Checksum offloading enable */
	uint16_t hw_timestamp:1;
	uint16_t vlan_strip:1;
	uint16_t crc_present:1; /* CRC flag */
	uint16_t rss_hash:1; /* RSS hash enabled */
	uint16_t rsv1:11;
};

uint16_t xsc_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n);
int xsc_rxq_elts_alloc(struct xsc_rxq_data *rxq_data);
int xsc_rxq_rss_obj_new(struct xsc_ethdev_priv *priv, uint16_t port_id);
void xsc_rxq_rss_obj_release(struct xsc_dev *xdev, struct xsc_rxq_data *rxq_data);
void xsc_rxq_elts_free(struct xsc_rxq_data *rxq_data);

#endif /* _XSC_RX_H_ */
