/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <rte_io.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_dev.h"
#include "xsc_ethdev.h"
#include "xsc_cmd.h"
#include "xsc_rx.h"

#define XSC_MAX_RECV_LEN 9800

static inline void
xsc_cq_to_mbuf(struct xsc_rxq_data *rxq, struct rte_mbuf *pkt,
	       volatile struct xsc_cqe *cqe)
{
	uint32_t rss_hash_res = 0;

	pkt->port = rxq->port_id;
	if (rxq->rss_hash) {
		rss_hash_res = rte_be_to_cpu_32(cqe->vni);
		if (rss_hash_res) {
			pkt->hash.rss = rss_hash_res;
			pkt->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		}
	}
}

static inline int
xsc_rx_poll_len(struct xsc_rxq_data *rxq, volatile struct xsc_cqe *cqe)
{
	int len;

	do {
		len = 0;
		int ret;

		ret = xsc_check_cqe_own(cqe, rxq->cqe_n, rxq->cq_ci);
		if (unlikely(ret != XSC_CQE_OWNER_SW)) {
			if (unlikely(ret == XSC_CQE_OWNER_ERR)) {
				++rxq->stats.rx_errors;
			} else {
				return 0;
			}
		}

		rxq->cq_ci += 1;
		len = rte_le_to_cpu_32(cqe->msg_len);
		return len;
	} while (1);
}

static __rte_always_inline void
xsc_pkt_info_sync(struct rte_mbuf *rep, struct rte_mbuf *seg)
{
	if (rep != NULL && seg != NULL) {
		rep->data_len = seg->data_len;
		rep->pkt_len = seg->pkt_len;
		rep->data_off = seg->data_off;
		rep->port = seg->port;
	}
}

uint16_t
xsc_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct xsc_rxq_data *rxq = dpdk_rxq;
	const uint32_t wqe_m = rxq->wqe_m;
	const uint32_t cqe_m = rxq->cqe_m;
	const uint32_t sge_n = rxq->sge_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	volatile struct xsc_cqe *cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_m];
	uint32_t nb_pkts = 0;
	uint64_t nb_bytes = 0;
	uint32_t rq_ci = rxq->rq_ci;
	int len = 0;
	uint32_t cq_ci_two = 0;
	int valid_cqe_num = 0;
	int cqe_msg_len = 0;
	volatile struct xsc_cqe_u64 *cqe_u64 = NULL;
	struct rte_mbuf *rep;

	while (pkts_n) {
		uint32_t idx = rq_ci & wqe_m;
		volatile struct xsc_wqe_data_seg *wqe =
			&((volatile struct xsc_wqe_data_seg *)rxq->wqes)[idx << sge_n];

		seg = (*rxq->elts)[idx];
		rte_prefetch0(cqe);
		rte_prefetch0(wqe);

		rep = rte_mbuf_raw_alloc(seg->pool);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			break;
		}

		if (!pkt) {
			if (valid_cqe_num) {
				cqe = cqe + 1;
				len = cqe_msg_len;
				valid_cqe_num = 0;
			} else if ((rxq->cq_ci % 2 == 0) && (pkts_n > 1)) {
				cq_ci_two = (rxq->cq_ci & rxq->cqe_m) / 2;
				cqe_u64 = &(*rxq->cqes_u64)[cq_ci_two];
				cqe = (volatile struct xsc_cqe *)cqe_u64;
				len = xsc_rx_poll_len(rxq, cqe);
				if (len > 0) {
					cqe_msg_len = xsc_rx_poll_len(rxq, cqe + 1);
					if (cqe_msg_len > 0)
						valid_cqe_num = 1;
				}
			} else {
				cqe = &(*rxq->cqes)[rxq->cq_ci & rxq->cqe_m];
				len = xsc_rx_poll_len(rxq, cqe);
			}

			if (!len) {
				rte_mbuf_raw_free(rep);
				break;
			}

			if (len > rte_pktmbuf_data_len(seg)) {
				rte_mbuf_raw_free(rep);
				pkt = NULL;
				++rq_ci;
				continue;
			}

			pkt = seg;
			pkt->ol_flags &= RTE_MBUF_F_EXTERNAL;
			xsc_cq_to_mbuf(rxq, pkt, cqe);

			if (rxq->crc_present)
				len -= RTE_ETHER_CRC_LEN;
			rte_pktmbuf_pkt_len(pkt) = len;
		}

		xsc_pkt_info_sync(rep, seg);
		(*rxq->elts)[idx] = rep;

		/* Fill wqe */
		wqe->va = rte_cpu_to_le_64(rte_pktmbuf_iova(rep));
		rte_pktmbuf_data_len(seg) = len;
		nb_bytes += rte_pktmbuf_pkt_len(pkt);

		*(pkts++) = pkt;
		pkt = NULL;
		--pkts_n;
		++nb_pkts;
		++rq_ci;
	}

	if (unlikely(nb_pkts == 0 && rq_ci == rxq->rq_ci))
		return 0;

	rxq->rq_ci = rq_ci;
	rxq->nb_rx_hold += nb_pkts;

	if (rxq->nb_rx_hold >= rxq->rx_free_thresh) {
		union xsc_cq_doorbell cq_db = {
			.cq_data = 0
		};
		cq_db.next_cid = rxq->cq_ci;
		cq_db.cq_num = rxq->cqn;

		union xsc_recv_doorbell rq_db = {
			.recv_data = 0
		};
		rq_db.next_pid = (rxq->rq_ci << sge_n);
		rq_db.qp_num = rxq->qpn;

		rte_write32(rte_cpu_to_le_32(cq_db.cq_data), rxq->cq_db);
		rte_write32(rte_cpu_to_le_32(rq_db.recv_data), rxq->rq_db);
		rxq->nb_rx_hold = 0;
	}

	rxq->stats.rx_pkts += nb_pkts;
	rxq->stats.rx_bytes += nb_bytes;

	return nb_pkts;
}

static void
xsc_rxq_initialize(struct xsc_dev *xdev, struct xsc_rxq_data *rxq_data)
{
	const uint32_t wqe_n = rxq_data->wqe_s;
	uint32_t i;
	uint32_t seg_len = 0;
	struct xsc_hwinfo *hwinfo = &xdev->hwinfo;
	uint32_t rx_ds_num = hwinfo->recv_seg_num;
	uint32_t log2ds = rte_log2_u32(rx_ds_num);
	uintptr_t addr;
	struct rte_mbuf *mbuf;
	void *jumbo_buffer_pa = xdev->jumbo_buffer_pa;
	void *jumbo_buffer_va = xdev->jumbo_buffer_va;
	volatile struct xsc_wqe_data_seg *seg;
	volatile struct xsc_wqe_data_seg *seg_next;

	for (i = 0; (i != wqe_n); ++i) {
		mbuf = (*rxq_data->elts)[i];
		seg = &((volatile struct xsc_wqe_data_seg *)rxq_data->wqes)[i * rx_ds_num];
		addr = (uintptr_t)rte_pktmbuf_iova(mbuf);
		if (rx_ds_num == 1)
			seg_len = XSC_MAX_RECV_LEN;
		else
			seg_len = rte_pktmbuf_data_len(mbuf);
		*seg = (struct xsc_wqe_data_seg){
			.va = rte_cpu_to_le_64(addr),
			.seg_len = rte_cpu_to_le_32(seg_len),
			.lkey = 0,
		};

		if (rx_ds_num != 1) {
			seg_next = seg + 1;
			if (jumbo_buffer_va == NULL) {
				jumbo_buffer_pa = rte_malloc(NULL, XSC_MAX_RECV_LEN, 0);
				if (jumbo_buffer_pa == NULL) {
					/* Rely on mtu */
					seg->seg_len = XSC_MAX_RECV_LEN;
					PMD_DRV_LOG(ERR, "Failed to malloc jumbo_buffer");
					continue;
				} else {
					jumbo_buffer_va =
						(void *)rte_malloc_virt2iova(jumbo_buffer_pa);
					if ((rte_iova_t)jumbo_buffer_va == RTE_BAD_IOVA) {
						seg->seg_len = XSC_MAX_RECV_LEN;
						PMD_DRV_LOG(ERR, "Failed to turn jumbo_buffer");
						continue;
					}
				}
				xdev->jumbo_buffer_pa = jumbo_buffer_pa;
				xdev->jumbo_buffer_va = jumbo_buffer_va;
			}
			*seg_next = (struct xsc_wqe_data_seg){
				.va = rte_cpu_to_le_64((uint64_t)jumbo_buffer_va),
				.seg_len = rte_cpu_to_le_32(XSC_MAX_RECV_LEN - seg_len),
				.lkey = 0,
			};
		}
	}

	rxq_data->rq_ci = wqe_n;
	rxq_data->sge_n = log2ds;

	union xsc_recv_doorbell recv_db = {
		.recv_data = 0
	};

	recv_db.next_pid = wqe_n << log2ds;
	recv_db.qp_num = rxq_data->qpn;
	rte_write32(rte_cpu_to_le_32(recv_db.recv_data), rxq_data->rq_db);
}

static int
xsc_rss_qp_create(struct xsc_ethdev_priv *priv, int port_id)
{
	struct xsc_cmd_create_multiqp_mbox_in *in;
	struct xsc_cmd_create_qp_request *req;
	struct xsc_cmd_create_multiqp_mbox_out *out;
	uint8_t log_ele;
	uint64_t iova;
	int wqe_n;
	int in_len, out_len, cmd_len;
	int entry_total_len, entry_len;
	uint8_t log_rq_sz, log_sq_sz = 0;
	uint32_t wqe_total_len;
	int j, ret;
	uint16_t i, pa_num;
	int rqn_base;
	struct xsc_rxq_data *rxq_data;
	struct xsc_dev *xdev = priv->xdev;
	struct xsc_hwinfo *hwinfo = &xdev->hwinfo;
	char name[RTE_ETH_NAME_MAX_LEN] = { 0 };
	void *cmd_buf;

	rxq_data = xsc_rxq_get(priv, 0);
	if (rxq_data == NULL)
		return -EINVAL;

	log_ele = rte_log2_u32(sizeof(struct xsc_wqe_data_seg));
	wqe_n = rxq_data->wqe_s;
	log_rq_sz = rte_log2_u32(wqe_n * hwinfo->recv_seg_num);
	wqe_total_len = 1 << (log_rq_sz + log_sq_sz + log_ele);

	pa_num = (wqe_total_len + XSC_PAGE_SIZE - 1) / XSC_PAGE_SIZE;
	entry_len = sizeof(struct xsc_cmd_create_qp_request) + sizeof(uint64_t) * pa_num;
	entry_total_len = entry_len * priv->num_rq;

	in_len = sizeof(struct xsc_cmd_create_multiqp_mbox_in) + entry_total_len;
	out_len = sizeof(struct xsc_cmd_create_multiqp_mbox_out) + entry_total_len;
	cmd_len = RTE_MAX(in_len, out_len);
	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Alloc rss qp create cmd memory failed");
		goto error;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	in->qp_num = rte_cpu_to_be_16((uint16_t)priv->num_rq);
	in->qp_type = XSC_QUEUE_TYPE_RAW;
	in->req_len = rte_cpu_to_be_32(cmd_len);

	for (i = 0; i < priv->num_rq; i++) {
		rxq_data = xsc_rxq_get(priv, i);
		if (rxq_data == NULL) {
			rte_errno = EINVAL;
			goto error;
		}

		req = (struct xsc_cmd_create_qp_request *)(&in->data[0] + entry_len * i);
		req->input_qpn = rte_cpu_to_be_16(0); /* useless for eth */
		req->pa_num = rte_cpu_to_be_16(pa_num);
		req->qp_type = XSC_QUEUE_TYPE_RAW;
		req->log_rq_sz = log_rq_sz;
		req->cqn_recv = rte_cpu_to_be_16((uint16_t)rxq_data->cqn);
		req->cqn_send = req->cqn_recv;
		req->glb_funcid = rte_cpu_to_be_16((uint16_t)hwinfo->func_id);
		/* Alloc pas addr */
		snprintf(name, sizeof(name), "wqe_mem_rx_%d_%d", port_id, i);
		rxq_data->rq_pas = rte_memzone_reserve_aligned(name,
							       (XSC_PAGE_SIZE * pa_num),
							       SOCKET_ID_ANY,
							       0, XSC_PAGE_SIZE);
		if (rxq_data->rq_pas == NULL) {
			rte_errno = ENOMEM;
			PMD_DRV_LOG(ERR, "Alloc rxq pas memory failed");
			goto error;
		}

		iova = rxq_data->rq_pas->iova;
		for (j = 0; j < pa_num; j++)
			req->pas[j] = rte_cpu_to_be_64(iova + j * XSC_PAGE_SIZE);
	}

	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_CREATE_MULTI_QP);
	out = cmd_buf;
	ret = xsc_dev_mailbox_exec(xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR,
			    "Create rss rq failed, port id=%d, qp_num=%d, ret=%d, out.status=%u",
			    port_id, priv->num_rq, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		goto error;
	}
	rqn_base = rte_be_to_cpu_32(out->qpn_base) & 0xffffff;

	for (i = 0; i < priv->num_rq; i++) {
		rxq_data = xsc_rxq_get(priv, i);
		if (rxq_data == NULL) {
			rte_errno = EINVAL;
			goto error;
		}

		rxq_data->wqes = rxq_data->rq_pas->addr;
		if (!xsc_dev_is_vf(xdev))
			rxq_data->rq_db = (uint32_t *)((uint8_t *)xdev->bar_addr +
					  XSC_PF_RX_DB_ADDR);
		else
			rxq_data->rq_db = (uint32_t *)((uint8_t *)xdev->bar_addr +
					  XSC_VF_RX_DB_ADDR);

		rxq_data->qpn = rqn_base + i;
		xsc_dev_modify_qp_status(xdev, rxq_data->qpn, 1, XSC_CMD_OP_RTR2RTS_QP);
		xsc_rxq_initialize(xdev, rxq_data);
		rxq_data->cq_ci = 0;
		priv->dev_data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
		PMD_DRV_LOG(INFO, "Port %d create rx qp, wqe_s:%d, wqe_n:%d, qp_db=%p, qpn:%u",
			    port_id,
			    rxq_data->wqe_s, rxq_data->wqe_n,
			    rxq_data->rq_db, rxq_data->qpn);
	}

	free(cmd_buf);
	return 0;

error:
	free(cmd_buf);
	return -rte_errno;
}

int
xsc_rxq_rss_obj_new(struct xsc_ethdev_priv *priv, uint16_t port_id)
{
	int ret;
	uint32_t i;
	struct xsc_dev *xdev = priv->xdev;
	struct xsc_rxq_data *rxq_data;
	struct xsc_rx_cq_params cq_params = {0};
	struct xsc_rx_cq_info cq_info = {0};

	/* Create CQ */
	for (i = 0; i < priv->num_rq; ++i) {
		rxq_data = xsc_rxq_get(priv, i);
		if (rxq_data == NULL)
			return -EINVAL;

		memset(&cq_params, 0, sizeof(cq_params));
		memset(&cq_info, 0, sizeof(cq_info));
		cq_params.port_id = rxq_data->port_id;
		cq_params.qp_id = rxq_data->idx;
		cq_params.wqe_s = rxq_data->wqe_s;

		ret = xsc_dev_rx_cq_create(xdev, &cq_params, &cq_info);
		if (ret) {
			PMD_DRV_LOG(ERR, "Port %u rxq %u create cq fail", port_id, i);
			rte_errno = errno;
			goto error;
		}

		rxq_data->cq = cq_info.cq;
		rxq_data->cqe_n = cq_info.cqe_n;
		rxq_data->cqe_s = 1 << rxq_data->cqe_n;
		rxq_data->cqe_m = rxq_data->cqe_s - 1;
		rxq_data->cqes = cq_info.cqes;
		rxq_data->cq_db = cq_info.cq_db;
		rxq_data->cqn = cq_info.cqn;

		PMD_DRV_LOG(INFO, "Port %u create rx cq, cqe_s:%d, cqe_n:%d, cq_db=%p, cqn:%u",
			    port_id,
			    rxq_data->cqe_s, rxq_data->cqe_n,
			    rxq_data->cq_db, rxq_data->cqn);
	}

	ret = xsc_rss_qp_create(priv, port_id);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Port %u rss rxq create fail", port_id);
		goto error;
	}
	return 0;

error:
	return -rte_errno;
}

int
xsc_rxq_elts_alloc(struct xsc_rxq_data *rxq_data)
{
	uint32_t elts_s = rxq_data->wqe_s;
	struct rte_mbuf *mbuf;
	uint32_t i;

	for (i = 0; (i != elts_s); ++i) {
		mbuf = rte_pktmbuf_alloc(rxq_data->mp);
		if (mbuf == NULL) {
			PMD_DRV_LOG(ERR, "Port %u rxq %u empty mbuf pool",
				    rxq_data->port_id, rxq_data->idx);
			rte_errno = ENOMEM;
			goto error;
		}

		mbuf->port = rxq_data->port_id;
		mbuf->nb_segs = 1;
		rte_pktmbuf_data_len(mbuf) = mbuf->buf_len - mbuf->data_off;
		rte_pktmbuf_pkt_len(mbuf) = rte_pktmbuf_data_len(mbuf);
		(*rxq_data->elts)[i] = mbuf;
	}

	return 0;
error:
	elts_s = i;
	for (i = 0; (i != elts_s); ++i) {
		if ((*rxq_data->elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq_data->elts)[i]);
		(*rxq_data->elts)[i] = NULL;
	}

	PMD_DRV_LOG(ERR, "Port %u rxq %u start failed, free elts",
		    rxq_data->port_id, rxq_data->idx);

	return -rte_errno;
}

void
xsc_rxq_elts_free(struct xsc_rxq_data *rxq_data)
{
	uint16_t i;

	if (rxq_data->elts == NULL)
		return;
	for (i = 0; i != rxq_data->wqe_s; ++i) {
		if ((*rxq_data->elts)[i] != NULL)
			rte_pktmbuf_free_seg((*rxq_data->elts)[i]);
		(*rxq_data->elts)[i] = NULL;
	}

	PMD_DRV_LOG(DEBUG, "Port %u rxq %u free elts", rxq_data->port_id, rxq_data->idx);
}

void
xsc_rxq_rss_obj_release(struct xsc_dev *xdev, struct xsc_rxq_data *rxq_data)
{
	struct xsc_cmd_destroy_qp_mbox_in in = { .hdr = { 0 } };
	struct xsc_cmd_destroy_qp_mbox_out out = { .hdr = { 0 } };
	int ret, in_len, out_len;
	uint32_t qpn = rxq_data->qpn;

	xsc_dev_modify_qp_status(xdev, qpn, 1, XSC_CMD_OP_QP_2RST);

	in_len = sizeof(struct xsc_cmd_destroy_qp_mbox_in);
	out_len = sizeof(struct xsc_cmd_destroy_qp_mbox_out);
	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_DESTROY_QP);
	in.qpn = rte_cpu_to_be_32(rxq_data->qpn);

	ret = xsc_dev_mailbox_exec(xdev, &in, in_len, &out, out_len);
	if (ret != 0 || out.hdr.status != 0) {
		PMD_DRV_LOG(ERR,
			    "Release rss rq failed, port id=%d, qid=%d, err=%d, out.status=%u",
			    rxq_data->port_id, rxq_data->idx, ret, out.hdr.status);
		rte_errno = ENOEXEC;
		return;
	}

	rte_memzone_free(rxq_data->rq_pas);

	if (rxq_data->cq != NULL)
		xsc_dev_destroy_cq(xdev, rxq_data->cq);
	rxq_data->cq = NULL;
}
