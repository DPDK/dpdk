/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_TX_H_
#define _XSC_TX_H_

#define XSC_TX_COMP_CQE_HANDLE_MAX	2

struct xsc_txq_stats {
	uint64_t tx_pkts;   /* Total number of tx packets */
	uint64_t tx_bytes;  /* Total number of tx bytes */
	uint64_t tx_errors; /* Total number of tx error packets */
};

struct __rte_cache_aligned xsc_txq_data {
	uint16_t idx;  /*QP idx */
	uint16_t port_id;
	void *cq; /* CQ pointer */
	void *qp; /* QP pointer */
	uint32_t cqn; /* CQ serial number */
	uint32_t qpn; /* QP serial number */
	uint16_t elts_head; /* Current pos in (*elts)[] */
	uint16_t elts_tail; /* Counter of first element awaiting completion */
	uint16_t elts_comp; /* Elts index since last completion request */
	uint16_t elts_s; /* Number of (*elts)[] */
	uint16_t elts_m; /* Mask of (*elts)[] number */
	uint16_t wqe_ci; /* Consumer index for TXQ */
	uint16_t wqe_pi; /* Producer index for TXQ */
	uint16_t wqe_s; /* Number of WQE */
	uint16_t wqe_m; /* Mask of WQE number */
	uint16_t wqe_comp; /* WQE index since last completion request */
	uint16_t cq_ci; /* Consumer index for CQ */
	uint16_t cq_pi; /* Production index for CQ */
	uint16_t cqe_s; /* Number of CQE */
	uint16_t cqe_m; /* Mask of CQE number */
	uint16_t elts_n:4; /* Log 2 of (*elts)[] number */
	uint16_t cqe_n:4; /* Log 2 of CQE number */
	uint16_t wqe_n:4; /* Log 2 of WQE number */
	uint16_t wqe_ds_n:4; /* Log 2 of each WQE DS number */
	uint64_t offloads; /* TXQ offloads */
	struct xsc_wqe *wqes;
	volatile struct xsc_cqe *cqes;
	volatile uint32_t *qp_db;
	volatile uint32_t *cq_db;
	struct xsc_ethdev_priv *priv;
	struct xsc_txq_stats stats;
	uint32_t socket;
	uint8_t tso_en:1; /* TSO enable 0-off 1-on */
	uint8_t rsv:7;
	uint16_t *fcqs; /* Free completion queue. */
	struct rte_mbuf *elts[]; /* Storage for queued packets, for free */
};

uint16_t xsc_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n);
int xsc_txq_obj_new(struct xsc_dev *xdev, struct xsc_txq_data *txq_data,
		    uint64_t offloads, uint16_t idx);
void xsc_txq_elts_alloc(struct xsc_txq_data *txq_data);
void xsc_txq_obj_release(struct xsc_dev *xdev, struct xsc_txq_data *txq_data);
void xsc_txq_elts_free(struct xsc_txq_data *txq_data);

#endif /* _XSC_TX_H_ */
