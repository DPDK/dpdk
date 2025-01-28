/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <rte_io.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_dev.h"
#include "xsc_ethdev.h"
#include "xsc_cmd.h"
#include "xsc_tx.h"
#include "xsc_np.h"

void
xsc_txq_elts_alloc(struct xsc_txq_data *txq_data)
{
	const uint32_t elts_s = 1 << txq_data->elts_n;
	uint32_t i;

	for (i = 0; i < elts_s; ++i)
		txq_data->elts[i] = NULL;
	txq_data->elts_head = 0;
	txq_data->elts_tail = 0;
	txq_data->elts_comp = 0;
}

int
xsc_txq_obj_new(struct xsc_dev *xdev, struct xsc_txq_data *txq_data,
		uint64_t offloads, uint16_t idx)
{
	int ret = 0;
	struct xsc_tx_cq_params cq_params = {0};
	struct xsc_tx_cq_info cq_info = {0};
	struct xsc_tx_qp_params qp_params = {0};
	struct xsc_tx_qp_info qp_info = {0};

	cq_params.port_id = txq_data->port_id;
	cq_params.qp_id = txq_data->idx;
	cq_params.elts_n = txq_data->elts_n;
	ret = xsc_dev_tx_cq_create(xdev, &cq_params, &cq_info);
	if (ret) {
		rte_errno = errno;
		goto error;
	}

	txq_data->cq = cq_info.cq;
	txq_data->cqe_n = cq_info.cqe_n;
	txq_data->cqe_s = cq_info.cqe_s;
	txq_data->cq_db = cq_info.cq_db;
	txq_data->cqn = cq_info.cqn;
	txq_data->cqes = cq_info.cqes;
	txq_data->cqe_m = txq_data->cqe_s - 1;

	PMD_DRV_LOG(INFO, "Create tx cq, cqe_s:%d, cqe_n:%d, cq_db=%p, cqn:%d",
		    txq_data->cqe_s, txq_data->cqe_n,
		    txq_data->cq_db, txq_data->cqn);

	qp_params.cq = txq_data->cq;
	qp_params.tx_offloads = offloads;
	qp_params.port_id = txq_data->port_id;
	qp_params.qp_id = idx;
	qp_params.elts_n = txq_data->elts_n;
	ret = xsc_dev_tx_qp_create(xdev, &qp_params, &qp_info);

	if (ret != 0) {
		rte_errno = errno;
		goto error;
	}

	txq_data->qp = qp_info.qp;
	txq_data->qpn = qp_info.qpn;
	txq_data->wqes = qp_info.wqes;
	txq_data->wqe_n = qp_info.wqe_n;
	txq_data->wqe_s = 1 << txq_data->wqe_n;
	txq_data->wqe_m = txq_data->wqe_s - 1;
	txq_data->wqe_ds_n = rte_log2_u32(xdev->hwinfo.send_seg_num);
	txq_data->qp_db =  qp_info.qp_db;

	txq_data->cq_ci = 0;
	txq_data->cq_pi = 0;
	txq_data->wqe_ci = 0;
	txq_data->wqe_pi = 0;
	txq_data->wqe_comp = 0;

	PMD_DRV_LOG(INFO, "Create tx qp, wqe_s:%d, wqe_n:%d, qp_db=%p, qpn:%d",
		    txq_data->wqe_s, txq_data->wqe_n,
		    txq_data->qp_db, txq_data->qpn);
	return 0;

error:
	return -rte_errno;
}

void
xsc_txq_obj_release(struct xsc_dev *xdev, struct xsc_txq_data *txq_data)
{
	PMD_DRV_LOG(DEBUG, "Destroy tx queue %u, portid %u",
		    txq_data->idx, txq_data->port_id);
	if (txq_data->qp != NULL)
		xsc_dev_destroy_qp(xdev, txq_data->qp);
	if (txq_data->cq != NULL)
		xsc_dev_destroy_cq(xdev, txq_data->cq);
}

void
xsc_txq_elts_free(struct xsc_txq_data *txq_data)
{
	const uint16_t elts_n = 1 << txq_data->elts_n;
	const uint16_t elts_m = elts_n - 1;
	uint16_t elts_head = txq_data->elts_head;
	uint16_t elts_tail = txq_data->elts_tail;
	struct rte_mbuf *(*elts)[elts_n] = &txq_data->elts;

	txq_data->elts_head = 0;
	txq_data->elts_tail = 0;
	txq_data->elts_comp = 0;

	while (elts_tail != elts_head) {
		struct rte_mbuf *elt = (*elts)[elts_tail & elts_m];

		rte_pktmbuf_free_seg(elt);
		++elts_tail;
	}
	PMD_DRV_LOG(DEBUG, "Port %u txq %u free elts", txq_data->port_id, txq_data->idx);
}
