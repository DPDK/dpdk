/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Intel Corporation
 */

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_hexdump.h>
#include <rte_comp.h>
#include <bus_pci_driver.h>
#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_common.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>

#include "qat_logs.h"
#include "qat_comp.h"
#include "qat_comp_pmd.h"

static void
qat_comp_fallback_to_fixed(struct icp_qat_fw_comp_req *comp_req)
{
	QAT_DP_LOG(DEBUG, "QAT PMD: fallback to fixed compression!");

	comp_req->comn_hdr.service_cmd_id =
			ICP_QAT_FW_COMP_CMD_STATIC;

	ICP_QAT_FW_COMN_NEXT_ID_SET(
			&comp_req->comp_cd_ctrl,
			ICP_QAT_FW_SLICE_DRAM_WR);

	ICP_QAT_FW_COMN_NEXT_ID_SET(
			&comp_req->u2.xlt_cd_ctrl,
			ICP_QAT_FW_SLICE_NULL);
	ICP_QAT_FW_COMN_CURR_ID_SET(
			&comp_req->u2.xlt_cd_ctrl,
			ICP_QAT_FW_SLICE_NULL);
}

void
qat_comp_free_split_op_memzones(struct qat_comp_op_cookie *cookie,
				unsigned int nb_children)
{
	unsigned int i;

	/* free all memzones allocated for child descriptors */
	for (i = 0; i < nb_children; i++)
		rte_memzone_free(cookie->dst_memzones[i]);

	/* and free the pointer table */
	rte_free(cookie->dst_memzones);
	cookie->dst_memzones = NULL;
}

static int
qat_comp_allocate_split_op_memzones(struct qat_comp_op_cookie *cookie,
				    unsigned int nb_descriptors_needed)
{
	struct qat_queue *txq = &(cookie->qp->tx_q);
	char dst_memz_name[RTE_MEMZONE_NAMESIZE];
	unsigned int i;

	/* allocate the array of memzone pointers */
	cookie->dst_memzones = rte_zmalloc_socket("qat PMD im buf mz pointers",
			(nb_descriptors_needed - 1) *
				sizeof(const struct rte_memzone *),
			RTE_CACHE_LINE_SIZE, cookie->socket_id);

	if (cookie->dst_memzones == NULL) {
		QAT_DP_LOG(ERR,
			"QAT PMD: failed to allocate im buf mz pointers");
		return -ENOMEM;
	}

	for (i = 0; i < nb_descriptors_needed - 1; i++) {
		snprintf(dst_memz_name,
				sizeof(dst_memz_name),
				"dst_%u_%u_%u_%u_%u",
				cookie->qp->qat_dev->qat_dev_id,
				txq->hw_bundle_number, txq->hw_queue_number,
				cookie->cookie_index, i);

		cookie->dst_memzones[i] = rte_memzone_reserve_aligned(
				dst_memz_name, RTE_PMD_QAT_COMP_IM_BUFFER_SIZE,
				cookie->socket_id, RTE_MEMZONE_IOVA_CONTIG,
				RTE_CACHE_LINE_SIZE);

		if (cookie->dst_memzones[i] == NULL) {
			QAT_DP_LOG(ERR,
				"QAT PMD: failed to allocate dst buffer memzone");

			/* let's free all memzones allocated up to now */
			qat_comp_free_split_op_memzones(cookie, i);

			return -ENOMEM;
		}
	}

	return 0;
}

int
qat_comp_build_request(void *in_op, uint8_t *out_msg,
		       void *op_cookie,
		       enum qat_device_gen qat_dev_gen __rte_unused)
{
	struct rte_comp_op *op = in_op;
	struct qat_comp_op_cookie *cookie =
			(struct qat_comp_op_cookie *)op_cookie;
	struct qat_comp_stream *stream;
	struct qat_comp_xform *qat_xform;
	const uint8_t *tmpl;
	struct icp_qat_fw_comp_req *comp_req =
	    (struct icp_qat_fw_comp_req *)out_msg;

	if (op->op_type == RTE_COMP_OP_STATEFUL) {
		stream = op->stream;
		qat_xform = &stream->qat_xform;
		if (unlikely(qat_xform->qat_comp_request_type !=
			     QAT_COMP_REQUEST_DECOMPRESS)) {
			QAT_DP_LOG(ERR, "QAT PMD does not support stateful compression");
			op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			return -EINVAL;
		}
		if (unlikely(stream->op_in_progress)) {
			QAT_DP_LOG(ERR, "QAT PMD does not support running multiple stateful operations on the same stream at once");
			op->status = RTE_COMP_OP_STATUS_INVALID_STATE;
			return -EINVAL;
		}
		stream->op_in_progress = 1;
	} else {
		stream = NULL;
		qat_xform = op->private_xform;
	}
	tmpl = (uint8_t *)&qat_xform->qat_comp_req_tmpl;

	rte_mov128(out_msg, tmpl);
	comp_req->comn_mid.opaque_data = (uint64_t)(uintptr_t)op;

	if (likely(qat_xform->qat_comp_request_type ==
			QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS)) {

		if (unlikely(op->src.length > QAT_FALLBACK_THLD)) {
			/* the operation must be split into pieces */
			if (qat_xform->checksum_type !=
					RTE_COMP_CHECKSUM_NONE) {
				/* fallback to fixed compression in case any
				 * checksum calculation was requested
				 */
				qat_comp_fallback_to_fixed(comp_req);
			} else {
				/* calculate num. of descriptors for split op */
				unsigned int nb_descriptors_needed =
					op->src.length / QAT_FALLBACK_THLD + 1;
				/* allocate memzone for output data */
				if (qat_comp_allocate_split_op_memzones(
					       cookie, nb_descriptors_needed)) {
					/* out of memory, fallback to fixed */
					qat_comp_fallback_to_fixed(comp_req);
				} else {
					QAT_DP_LOG(DEBUG,
							"Input data is too big, op must be split into %u descriptors",
							nb_descriptors_needed);
					return (int) nb_descriptors_needed;
				}
			}
		}

		/* set BFINAL bit according to flush_flag */
		comp_req->comp_pars.req_par_flags =
			ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
				ICP_QAT_FW_COMP_SOP,
				ICP_QAT_FW_COMP_EOP,
				op->flush_flag == RTE_COMP_FLUSH_FINAL ?
					ICP_QAT_FW_COMP_BFINAL
					: ICP_QAT_FW_COMP_NOT_BFINAL,
				ICP_QAT_FW_COMP_CNV,
				ICP_QAT_FW_COMP_CNV_RECOVERY);

	} else if (op->op_type == RTE_COMP_OP_STATEFUL) {

		comp_req->comp_pars.req_par_flags =
			ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
				(stream->start_of_packet) ?
					ICP_QAT_FW_COMP_SOP
				      : ICP_QAT_FW_COMP_NOT_SOP,
				(op->flush_flag == RTE_COMP_FLUSH_FULL ||
				 op->flush_flag == RTE_COMP_FLUSH_FINAL) ?
					ICP_QAT_FW_COMP_EOP
				      : ICP_QAT_FW_COMP_NOT_EOP,
				ICP_QAT_FW_COMP_NOT_BFINAL,
				ICP_QAT_FW_COMP_CNV,
				ICP_QAT_FW_COMP_CNV_RECOVERY);
	}

	/* common for sgl and flat buffers */
	comp_req->comp_pars.comp_len = op->src.length;
	comp_req->comp_pars.out_buffer_sz = rte_pktmbuf_pkt_len(op->m_dst) -
			op->dst.offset;

	if (op->m_src->next != NULL || op->m_dst->next != NULL) {
		/* sgl */
		int ret = 0;

		ICP_QAT_FW_COMN_PTR_TYPE_SET(comp_req->comn_hdr.comn_req_flags,
				QAT_COMN_PTR_TYPE_SGL);

		if (unlikely(op->m_src->nb_segs > cookie->src_nb_elems)) {
			/* we need to allocate more elements in SGL*/
			void *tmp;

			tmp = rte_realloc_socket(cookie->qat_sgl_src_d,
					  sizeof(struct qat_sgl) +
					  sizeof(struct qat_flat_buf) *
					  op->m_src->nb_segs, 64,
					  cookie->socket_id);

			if (unlikely(tmp == NULL)) {
				QAT_DP_LOG(ERR, "QAT PMD can't allocate memory"
					   " for %d elements of SGL",
					   op->m_src->nb_segs);
				op->status = RTE_COMP_OP_STATUS_ERROR;
				/* clear op-in-progress flag */
				if (stream)
					stream->op_in_progress = 0;
				return -ENOMEM;
			}
			/* new SGL is valid now */
			cookie->qat_sgl_src_d = (struct qat_sgl *)tmp;
			cookie->src_nb_elems = op->m_src->nb_segs;
			cookie->qat_sgl_src_phys_addr =
				rte_malloc_virt2iova(cookie->qat_sgl_src_d);
		}

		ret = qat_sgl_fill_array(op->m_src,
				op->src.offset,
				cookie->qat_sgl_src_d,
				op->src.length,
				cookie->src_nb_elems);
		if (ret) {
			QAT_DP_LOG(ERR, "QAT PMD Cannot fill source sgl array");
			op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			/* clear op-in-progress flag */
			if (stream)
				stream->op_in_progress = 0;
			return ret;
		}

		if (unlikely(op->m_dst->nb_segs > cookie->dst_nb_elems)) {
			/* we need to allocate more elements in SGL*/
			struct qat_sgl *tmp;

			tmp = rte_realloc_socket(cookie->qat_sgl_dst_d,
					  sizeof(struct qat_sgl) +
					  sizeof(struct qat_flat_buf) *
					  op->m_dst->nb_segs, 64,
					  cookie->socket_id);

			if (unlikely(tmp == NULL)) {
				QAT_DP_LOG(ERR, "QAT PMD can't allocate memory"
					   " for %d elements of SGL",
					   op->m_dst->nb_segs);
				op->status = RTE_COMP_OP_STATUS_ERROR;
				/* clear op-in-progress flag */
				if (stream)
					stream->op_in_progress = 0;
				return -ENOMEM;
			}
			/* new SGL is valid now */
			cookie->qat_sgl_dst_d = (struct qat_sgl *)tmp;
			cookie->dst_nb_elems = op->m_dst->nb_segs;
			cookie->qat_sgl_dst_phys_addr =
				rte_malloc_virt2iova(cookie->qat_sgl_dst_d);
		}

		ret = qat_sgl_fill_array(op->m_dst,
				op->dst.offset,
				cookie->qat_sgl_dst_d,
				comp_req->comp_pars.out_buffer_sz,
				cookie->dst_nb_elems);
		if (ret) {
			QAT_DP_LOG(ERR, "QAT PMD Cannot fill dest. sgl array");
			op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			/* clear op-in-progress flag */
			if (stream)
				stream->op_in_progress = 0;
			return ret;
		}

		comp_req->comn_mid.src_data_addr =
				cookie->qat_sgl_src_phys_addr;
		comp_req->comn_mid.dest_data_addr =
				cookie->qat_sgl_dst_phys_addr;
		comp_req->comn_mid.src_length = 0;
		comp_req->comn_mid.dst_length = 0;

	} else {
		/* flat aka linear buffer */
		ICP_QAT_FW_COMN_PTR_TYPE_SET(comp_req->comn_hdr.comn_req_flags,
				QAT_COMN_PTR_TYPE_FLAT);
		comp_req->comn_mid.src_length = op->src.length;
		comp_req->comn_mid.dst_length =
				comp_req->comp_pars.out_buffer_sz;

		comp_req->comn_mid.src_data_addr =
		    rte_pktmbuf_iova_offset(op->m_src, op->src.offset);
		comp_req->comn_mid.dest_data_addr =
		    rte_pktmbuf_iova_offset(op->m_dst, op->dst.offset);
	}

	if (unlikely(rte_pktmbuf_pkt_len(op->m_dst) < QAT_MIN_OUT_BUF_SIZE)) {
		/* QAT doesn't support dest. buffer lower
		 * than QAT_MIN_OUT_BUF_SIZE. Propagate error mark
		 * by converting this request to the null one
		 * and check the status in the response.
		 */
		QAT_DP_LOG(WARNING, "QAT destination buffer too small - resend with larger buffer");
		comp_req->comn_hdr.service_type = ICP_QAT_FW_COMN_REQ_NULL;
		comp_req->comn_hdr.service_cmd_id = ICP_QAT_FW_NULL_REQ_SERV_ID;
		cookie->error = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
	}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_LOG(DEBUG, "Direction: %s",
	    qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS ?
			    "decompression" : "compression");
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat compression message:", comp_req,
		    sizeof(struct icp_qat_fw_comp_req));
#endif
	return 0;
}

static inline uint32_t
adf_modulo(uint32_t data, uint32_t modulo_mask)
{
	return data & modulo_mask;
}

static inline void
qat_comp_mbuf_skip(struct rte_mbuf **mbuf, uint32_t *offset, uint32_t len)
{
	while (*offset + len >= rte_pktmbuf_data_len(*mbuf)) {
		len -= (rte_pktmbuf_data_len(*mbuf) - *offset);
		*mbuf = (*mbuf)->next;
		*offset = 0;
	}
	*offset = len;
}

int
qat_comp_build_multiple_requests(void *in_op, struct qat_qp *qp,
				 uint32_t parent_tail, int nb_descr)
{
	struct rte_comp_op op_backup;
	struct rte_mbuf dst_mbuf;
	struct rte_comp_op *op = in_op;
	struct qat_queue *txq = &(qp->tx_q);
	uint8_t *base_addr = (uint8_t *)txq->base_addr;
	uint8_t *out_msg = base_addr + parent_tail;
	uint32_t tail = parent_tail;
	struct icp_qat_fw_comp_req *comp_req =
			(struct icp_qat_fw_comp_req *)out_msg;
	struct qat_comp_op_cookie *parent_cookie =
			(struct qat_comp_op_cookie *)
			qp->op_cookies[parent_tail / txq->msg_size];
	struct qat_comp_op_cookie *child_cookie;
	uint16_t dst_data_size =
			RTE_MIN(RTE_PMD_QAT_COMP_IM_BUFFER_SIZE, 65535);
	uint32_t data_to_enqueue = op->src.length - QAT_FALLBACK_THLD;
	int num_descriptors_built = 1;
	int ret;

	QAT_DP_LOG(DEBUG, "op %p, parent_cookie %p", op, parent_cookie);

	/* copy original op to the local variable for restoring later */
	rte_memcpy(&op_backup, op, sizeof(op_backup));

	parent_cookie->nb_child_responses = 0;
	parent_cookie->nb_children = 0;
	parent_cookie->split_op = 1;
	parent_cookie->dst_data = op->m_dst;
	parent_cookie->dst_data_offset = op->dst.offset;

	op->src.length = QAT_FALLBACK_THLD;
	op->flush_flag = RTE_COMP_FLUSH_FULL;

	QAT_DP_LOG(DEBUG, "parent op src len %u dst len %u",
			op->src.length, op->m_dst->pkt_len);

	ret = qat_comp_build_request(in_op, out_msg, parent_cookie,
			qp->qat_dev_gen);
	if (ret != 0) {
		/* restore op and clear cookie */
		QAT_DP_LOG(WARNING, "Failed to build parent descriptor");
		op->src.length = op_backup.src.length;
		op->flush_flag = op_backup.flush_flag;
		parent_cookie->split_op = 0;
		return ret;
	}

	/* prepare local dst mbuf */
	rte_memcpy(&dst_mbuf, op->m_dst, sizeof(dst_mbuf));
	rte_pktmbuf_reset(&dst_mbuf);
	dst_mbuf.buf_len = dst_data_size;
	dst_mbuf.data_len = dst_data_size;
	dst_mbuf.pkt_len = dst_data_size;
	dst_mbuf.data_off = 0;

	/* update op for the child operations */
	op->m_dst = &dst_mbuf;
	op->dst.offset = 0;

	while (data_to_enqueue) {
		const struct rte_memzone *mz =
			parent_cookie->dst_memzones[num_descriptors_built - 1];
		uint32_t src_data_size = RTE_MIN(data_to_enqueue,
				QAT_FALLBACK_THLD);
		uint32_t cookie_index;

		/* update params for the next op */
		op->src.offset += QAT_FALLBACK_THLD;
		op->src.length = src_data_size;
		op->flush_flag = (src_data_size == data_to_enqueue) ?
			op_backup.flush_flag : RTE_COMP_FLUSH_FULL;

		/* update dst mbuf for the next op (use memzone for dst data) */
		dst_mbuf.buf_addr = mz->addr;
		dst_mbuf.buf_iova = mz->iova;

		/* move the tail and calculate next cookie index */
		tail = adf_modulo(tail + txq->msg_size, txq->modulo_mask);
		cookie_index = tail / txq->msg_size;
		child_cookie = (struct qat_comp_op_cookie *)
				qp->op_cookies[cookie_index];
		comp_req = (struct icp_qat_fw_comp_req *)(base_addr + tail);

		/* update child cookie */
		child_cookie->split_op = 1; /* must be set for child as well */
		child_cookie->parent_cookie = parent_cookie; /* same as above */
		child_cookie->nb_children = 0;
		child_cookie->dest_buffer = mz->addr;

		QAT_DP_LOG(DEBUG,
				"cookie_index %u, child_cookie %p, comp_req %p",
				cookie_index, child_cookie, comp_req);
		QAT_DP_LOG(DEBUG,
				"data_to_enqueue %u, num_descriptors_built %d",
				data_to_enqueue, num_descriptors_built);
		QAT_DP_LOG(DEBUG, "child op src len %u dst len %u",
				op->src.length, op->m_dst->pkt_len);

		/* build the request */
		ret = qat_comp_build_request(op, (uint8_t *)comp_req,
				child_cookie, qp->qat_dev_gen);
		if (ret < 0) {
			QAT_DP_LOG(WARNING, "Failed to build child descriptor");
			/* restore op and clear cookie */
			rte_memcpy(op, &op_backup, sizeof(op_backup));
			parent_cookie->split_op = 0;
			parent_cookie->nb_children = 0;
			return ret;
		}

		data_to_enqueue -= src_data_size;
		num_descriptors_built++;
	}

	/* restore backed up original op */
	rte_memcpy(op, &op_backup, sizeof(op_backup));

	if (nb_descr != num_descriptors_built)
		QAT_DP_LOG(ERR, "split op. expected %d, built %d",
				nb_descr, num_descriptors_built);

	parent_cookie->nb_children = num_descriptors_built - 1;
	return num_descriptors_built;
}

static inline void
qat_comp_response_data_copy(struct qat_comp_op_cookie *cookie,
		       struct rte_comp_op *rx_op)
{
	struct qat_comp_op_cookie *pc = cookie->parent_cookie;
	struct rte_mbuf *sgl_buf = pc->dst_data;
	void *op_dst_addr = rte_pktmbuf_mtod_offset(sgl_buf, uint8_t *,
						    pc->dst_data_offset);

	/* number of bytes left in the current segment */
	uint32_t left_in_current = rte_pktmbuf_data_len(sgl_buf) -
			pc->dst_data_offset;

	uint32_t prod, sent;

	if (rx_op->produced <= left_in_current) {
		rte_memcpy(op_dst_addr, cookie->dest_buffer,
				rx_op->produced);
		/* calculate dst mbuf and offset for the next child op */
		if (rx_op->produced == left_in_current) {
			pc->dst_data = sgl_buf->next;
			pc->dst_data_offset = 0;
		} else
			pc->dst_data_offset += rx_op->produced;
	} else {
		rte_memcpy(op_dst_addr, cookie->dest_buffer,
				left_in_current);
		sgl_buf = sgl_buf->next;
		prod = rx_op->produced - left_in_current;
		sent = left_in_current;
		while (prod > rte_pktmbuf_data_len(sgl_buf)) {
			op_dst_addr = rte_pktmbuf_mtod_offset(sgl_buf,
					uint8_t *, 0);

			rte_memcpy(op_dst_addr,
					((uint8_t *)cookie->dest_buffer) +
					sent,
					rte_pktmbuf_data_len(sgl_buf));

			prod -= rte_pktmbuf_data_len(sgl_buf);
			sent += rte_pktmbuf_data_len(sgl_buf);

			sgl_buf = sgl_buf->next;
		}

		op_dst_addr = rte_pktmbuf_mtod_offset(sgl_buf, uint8_t *, 0);

		rte_memcpy(op_dst_addr,
				((uint8_t *)cookie->dest_buffer) + sent,
				prod);

		/* calculate dst mbuf and offset for the next child op */
		if (prod == rte_pktmbuf_data_len(sgl_buf)) {
			pc->dst_data = sgl_buf->next;
			pc->dst_data_offset = 0;
		} else {
			pc->dst_data = sgl_buf;
			pc->dst_data_offset = prod;
		}
	}
}

int
qat_comp_process_response(void **op, uint8_t *resp, void *op_cookie,
			  uint64_t *dequeue_err_count)
{
	struct icp_qat_fw_comp_resp *resp_msg =
			(struct icp_qat_fw_comp_resp *)resp;
	struct qat_comp_op_cookie *cookie =
			(struct qat_comp_op_cookie *)op_cookie;

	struct icp_qat_fw_resp_comp_pars *comp_resp1 =
	  (struct icp_qat_fw_resp_comp_pars *)&resp_msg->comp_resp_pars;

	QAT_DP_LOG(DEBUG, "input counter = %u, output counter = %u",
		   comp_resp1->input_byte_counter,
		   comp_resp1->output_byte_counter);

	struct rte_comp_op *rx_op = (struct rte_comp_op *)(uintptr_t)
			(resp_msg->opaque_data);
	struct qat_comp_stream *stream;
	struct qat_comp_xform *qat_xform;
	int err = resp_msg->comn_resp.comn_status &
			((1 << QAT_COMN_RESP_CMP_STATUS_BITPOS) |
			 (1 << QAT_COMN_RESP_XLAT_STATUS_BITPOS));

	if (rx_op->op_type == RTE_COMP_OP_STATEFUL) {
		stream = rx_op->stream;
		qat_xform = &stream->qat_xform;
		/* clear op-in-progress flag */
		stream->op_in_progress = 0;
	} else {
		stream = NULL;
		qat_xform = rx_op->private_xform;
	}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_LOG(DEBUG, "Direction: %s",
	    qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS ?
	    "decompression" : "compression");
	QAT_DP_HEXDUMP_LOG(DEBUG,  "qat_response:", (uint8_t *)resp_msg,
			sizeof(struct icp_qat_fw_comp_resp));
#endif

	if (unlikely(cookie->error)) {
		rx_op->status = cookie->error;
		cookie->error = 0;
		++(*dequeue_err_count);
		rx_op->debug_status = 0;
		rx_op->consumed = 0;
		rx_op->produced = 0;
		*op = (void *)rx_op;
		/* also in this case number of returned ops */
		/* must be equal to one, */
		/* appropriate status (error) must be set as well */
		return 1;
	}

	if (likely(qat_xform->qat_comp_request_type
			!= QAT_COMP_REQUEST_DECOMPRESS)) {
		if (unlikely(ICP_QAT_FW_COMN_HDR_CNV_FLAG_GET(
				resp_msg->comn_resp.hdr_flags)
					== ICP_QAT_FW_COMP_NO_CNV)) {
			rx_op->status = RTE_COMP_OP_STATUS_ERROR;
			rx_op->debug_status = ERR_CODE_QAT_COMP_WRONG_FW;
			*op = (void *)rx_op;
			QAT_DP_LOG(ERR,
					"This QAT hardware doesn't support compression operation");
			++(*dequeue_err_count);
			return 1;
		}
	}

	if (err) {
		if (unlikely((err & (1 << QAT_COMN_RESP_XLAT_STATUS_BITPOS))
			     &&	(qat_xform->qat_comp_request_type
				 == QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS))) {
			QAT_DP_LOG(ERR, "QAT intermediate buffer may be too "
			    "small for output, try configuring a larger size");
		}

		int8_t cmp_err_code =
			(int8_t)resp_msg->comn_resp.comn_error.cmp_err_code;
		int8_t xlat_err_code =
			(int8_t)resp_msg->comn_resp.comn_error.xlat_err_code;

		/* handle recoverable out-of-buffer condition in stateful
		 * decompression scenario
		 */
		if (cmp_err_code == ERR_CODE_OVERFLOW_ERROR && !xlat_err_code
				&& qat_xform->qat_comp_request_type
					== QAT_COMP_REQUEST_DECOMPRESS
				&& rx_op->op_type == RTE_COMP_OP_STATEFUL) {
			struct icp_qat_fw_resp_comp_pars *comp_resp =
					&resp_msg->comp_resp_pars;
			rx_op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE;
			rx_op->consumed = comp_resp->input_byte_counter;
			rx_op->produced = comp_resp->output_byte_counter;
			stream->start_of_packet = 0;
		} else if ((cmp_err_code == ERR_CODE_OVERFLOW_ERROR
			  && !xlat_err_code)
				||
		    (!cmp_err_code && xlat_err_code == ERR_CODE_OVERFLOW_ERROR)
				||
		    (cmp_err_code == ERR_CODE_OVERFLOW_ERROR &&
		     xlat_err_code == ERR_CODE_OVERFLOW_ERROR)){

			struct icp_qat_fw_resp_comp_pars *comp_resp =
					(struct icp_qat_fw_resp_comp_pars *)
					&resp_msg->comp_resp_pars;

			/* handle recoverable out-of-buffer condition
			 * in stateless compression scenario
			 */
			if (comp_resp->input_byte_counter) {
				if ((qat_xform->qat_comp_request_type
				== QAT_COMP_REQUEST_FIXED_COMP_STATELESS) ||
				    (qat_xform->qat_comp_request_type
				== QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS)) {

					rx_op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE;
					rx_op->consumed =
						comp_resp->input_byte_counter;
					rx_op->produced =
						comp_resp->output_byte_counter;
				} else
					rx_op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
			} else
				rx_op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
		} else
			rx_op->status = RTE_COMP_OP_STATUS_ERROR;

		++(*dequeue_err_count);
		rx_op->debug_status =
			*((uint16_t *)(&resp_msg->comn_resp.comn_error));
	} else {
		struct icp_qat_fw_resp_comp_pars *comp_resp =
		  (struct icp_qat_fw_resp_comp_pars *)&resp_msg->comp_resp_pars;

		rx_op->status = RTE_COMP_OP_STATUS_SUCCESS;
		rx_op->consumed = comp_resp->input_byte_counter;
		rx_op->produced = comp_resp->output_byte_counter;
		if (stream)
			stream->start_of_packet = 0;

		if (qat_xform->checksum_type != RTE_COMP_CHECKSUM_NONE) {
			if (qat_xform->checksum_type == RTE_COMP_CHECKSUM_CRC32)
				rx_op->output_chksum = comp_resp->curr_crc32;
			else if (qat_xform->checksum_type ==
					RTE_COMP_CHECKSUM_ADLER32)
				rx_op->output_chksum = comp_resp->curr_adler_32;
			else
				rx_op->output_chksum = comp_resp->curr_chksum;
		}
	}
	QAT_DP_LOG(DEBUG, "About to check for split op :cookies: %p %p, split:%u",
		cookie, cookie->parent_cookie, cookie->split_op);

	if (cookie->split_op) {
		*op = NULL;
		struct qat_comp_op_cookie *pc = cookie->parent_cookie;

		if (cookie->nb_children > 0) {
			QAT_DP_LOG(DEBUG, "Parent");
			/* parent - don't return until all children
			 * responses are collected
			 */
			cookie->total_consumed = rx_op->consumed;
			cookie->total_produced = rx_op->produced;
			if (err) {
				cookie->error = rx_op->status;
				rx_op->status = RTE_COMP_OP_STATUS_SUCCESS;
			} else {
				/* calculate dst mbuf and offset for child op */
				qat_comp_mbuf_skip(&cookie->dst_data,
						&cookie->dst_data_offset,
						rx_op->produced);
			}
		} else {
			QAT_DP_LOG(DEBUG, "Child");
			if (pc->error == RTE_COMP_OP_STATUS_SUCCESS) {
				if (err)
					pc->error = rx_op->status;
				if (rx_op->produced) {
					/* this covers both SUCCESS and
					 * OUT_OF_SPACE_RECOVERABLE cases
					 */
					qat_comp_response_data_copy(cookie,
							rx_op);
					pc->total_consumed += rx_op->consumed;
					pc->total_produced += rx_op->produced;
				}
			}
			rx_op->status = RTE_COMP_OP_STATUS_SUCCESS;

			pc->nb_child_responses++;

			/* (child) cookie fields have to be reset
			 * to avoid problems with reusability -
			 * rx and tx queue starting from index zero
			 */
			cookie->nb_children = 0;
			cookie->split_op = 0;
			cookie->nb_child_responses = 0;
			cookie->dest_buffer = NULL;

			if (pc->nb_child_responses == pc->nb_children) {
				uint8_t child_resp;

				/* parent should be included as well */
				child_resp = pc->nb_child_responses + 1;

				rx_op->status = pc->error;
				rx_op->consumed = pc->total_consumed;
				rx_op->produced = pc->total_produced;
				*op = (void *)rx_op;

				/* free memzones used for dst data */
				qat_comp_free_split_op_memzones(pc,
						pc->nb_children);

				/* (parent) cookie fields have to be reset
				 * to avoid problems with reusability -
				 * rx and tx queue starting from index zero
				 */
				pc->nb_children = 0;
				pc->split_op = 0;
				pc->nb_child_responses = 0;
				pc->error = RTE_COMP_OP_STATUS_SUCCESS;

				return child_resp;
			}
		}
		return 0;
	}

	*op = (void *)rx_op;
	return 1;
}

unsigned int
qat_comp_xform_size(void)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_comp_xform), 8);
}

unsigned int
qat_comp_stream_size(void)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_comp_stream), 8);
}

static void
qat_comp_create_req_hdr(struct icp_qat_fw_comn_req_hdr *header,
	    enum qat_comp_request_type request)
{
	if (request == QAT_COMP_REQUEST_FIXED_COMP_STATELESS)
		header->service_cmd_id = ICP_QAT_FW_COMP_CMD_STATIC;
	else if (request == QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS)
		header->service_cmd_id = ICP_QAT_FW_COMP_CMD_DYNAMIC;
	else if (request == QAT_COMP_REQUEST_DECOMPRESS)
		header->service_cmd_id = ICP_QAT_FW_COMP_CMD_DECOMPRESS;

	header->service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_COMP;
	header->hdr_flags =
	    ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(ICP_QAT_FW_COMN_REQ_FLAG_SET);

	header->comn_req_flags = ICP_QAT_FW_COMN_FLAGS_BUILD(
	    QAT_COMN_CD_FLD_TYPE_16BYTE_DATA, QAT_COMN_PTR_TYPE_FLAT);
}

static int
qat_comp_create_templates(struct qat_comp_xform *qat_xform,
			  const struct rte_memzone *interm_buff_mz,
			  const struct rte_comp_xform *xform,
			  const struct qat_comp_stream *stream,
			  enum rte_comp_op_type op_type,
			  enum qat_device_gen qat_dev_gen)
{
	struct icp_qat_fw_comp_req *comp_req;
	uint32_t req_par_flags;
	int res;

	if (unlikely(qat_xform == NULL)) {
		QAT_LOG(ERR, "Session was not created for this device");
		return -EINVAL;
	}

	if (op_type == RTE_COMP_OP_STATEFUL) {
		if (unlikely(stream == NULL)) {
			QAT_LOG(ERR, "Stream must be non null for stateful op");
			return -EINVAL;
		}
		if (unlikely(qat_xform->qat_comp_request_type !=
			     QAT_COMP_REQUEST_DECOMPRESS)) {
			QAT_LOG(ERR, "QAT PMD does not support stateful compression");
			return -ENOTSUP;
		}
	}

	if (qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS)
		req_par_flags = ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
				ICP_QAT_FW_COMP_SOP, ICP_QAT_FW_COMP_EOP,
				ICP_QAT_FW_COMP_BFINAL,
				ICP_QAT_FW_COMP_CNV,
				ICP_QAT_FW_COMP_CNV_RECOVERY);
	else
		req_par_flags = ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
				ICP_QAT_FW_COMP_SOP, ICP_QAT_FW_COMP_EOP,
				ICP_QAT_FW_COMP_BFINAL, ICP_QAT_FW_COMP_CNV,
				ICP_QAT_FW_COMP_CNV_RECOVERY);

	comp_req = &qat_xform->qat_comp_req_tmpl;

	/* Initialize header */
	qat_comp_create_req_hdr(&comp_req->comn_hdr,
					qat_xform->qat_comp_request_type);

	if (op_type == RTE_COMP_OP_STATEFUL) {
		comp_req->comn_hdr.serv_specif_flags =
				ICP_QAT_FW_COMP_FLAGS_BUILD(
			ICP_QAT_FW_COMP_STATEFUL_SESSION,
			ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST,
			ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST,
			ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST,
			ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF);

		/* Decompression state registers */
		comp_req->comp_cd_ctrl.comp_state_addr =
				stream->state_registers_decomp_phys;

		/* RAM bank flags */
		comp_req->comp_cd_ctrl.ram_bank_flags =
				qat_comp_gen_dev_ops[qat_dev_gen]
					.qat_comp_get_ram_bank_flags();

		comp_req->comp_cd_ctrl.ram_banks_addr =
				stream->inflate_context_phys;
	} else {
		comp_req->comn_hdr.serv_specif_flags =
				ICP_QAT_FW_COMP_FLAGS_BUILD(
			ICP_QAT_FW_COMP_STATELESS_SESSION,
			ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST,
			ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST,
			ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST,
			ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF);
	}

	res = qat_comp_gen_dev_ops[qat_dev_gen].qat_comp_set_slice_cfg_word(
			qat_xform, xform, op_type,
			comp_req->cd_pars.sl.comp_slice_cfg_word);
	if (res)
		return res;

	comp_req->comp_pars.initial_adler = 1;
	comp_req->comp_pars.initial_crc32 = 0;
	comp_req->comp_pars.req_par_flags = req_par_flags;


	if (qat_xform->qat_comp_request_type ==
			QAT_COMP_REQUEST_FIXED_COMP_STATELESS ||
	    qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS) {
		ICP_QAT_FW_COMN_NEXT_ID_SET(&comp_req->comp_cd_ctrl,
					    ICP_QAT_FW_SLICE_DRAM_WR);
		ICP_QAT_FW_COMN_CURR_ID_SET(&comp_req->comp_cd_ctrl,
					    ICP_QAT_FW_SLICE_COMP);
	} else if (qat_xform->qat_comp_request_type ==
			QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS) {

		ICP_QAT_FW_COMN_NEXT_ID_SET(&comp_req->comp_cd_ctrl,
				ICP_QAT_FW_SLICE_XLAT);
		ICP_QAT_FW_COMN_CURR_ID_SET(&comp_req->comp_cd_ctrl,
				ICP_QAT_FW_SLICE_COMP);

		ICP_QAT_FW_COMN_NEXT_ID_SET(&comp_req->u2.xlt_cd_ctrl,
				ICP_QAT_FW_SLICE_DRAM_WR);
		ICP_QAT_FW_COMN_CURR_ID_SET(&comp_req->u2.xlt_cd_ctrl,
				ICP_QAT_FW_SLICE_XLAT);

		comp_req->u1.xlt_pars.inter_buff_ptr =
				(qat_comp_get_num_im_bufs_required(qat_dev_gen)
					== 0) ? 0 : interm_buff_mz->iova;
	}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat compression message template:", comp_req,
		    sizeof(struct icp_qat_fw_comp_req));
#endif
	return 0;
}

/**
 * Create driver private_xform data.
 *
 * @param dev
 *   Compressdev device
 * @param xform
 *   xform data from application
 * @param private_xform
 *   ptr where handle of pmd's private_xform data should be stored
 * @return
 *  - if successful returns 0
 *    and valid private_xform handle
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support the comp transform.
 *  - Returns -ENOMEM if the private_xform could not be allocated.
 */
int
qat_comp_private_xform_create(struct rte_compressdev *dev,
			      const struct rte_comp_xform *xform,
			      void **private_xform)
{
	struct qat_comp_dev_private *qat = dev->data->dev_private;
	enum qat_device_gen qat_dev_gen = qat->qat_dev->qat_dev_gen;
	unsigned int im_bufs = qat_comp_get_num_im_bufs_required(qat_dev_gen);

	if (unlikely(private_xform == NULL)) {
		QAT_LOG(ERR, "QAT: private_xform parameter is NULL");
		return -EINVAL;
	}
	if (unlikely(qat->xformpool == NULL)) {
		QAT_LOG(ERR, "QAT device has no private_xform mempool");
		return -ENOMEM;
	}
	if (rte_mempool_get(qat->xformpool, private_xform)) {
		QAT_LOG(ERR, "Couldn't get object from qat xform mempool");
		return -ENOMEM;
	}

	struct qat_comp_xform *qat_xform =
			(struct qat_comp_xform *)*private_xform;

	if (xform->type == RTE_COMP_COMPRESS) {

		if (xform->compress.deflate.huffman == RTE_COMP_HUFFMAN_FIXED ||
		  ((xform->compress.deflate.huffman == RTE_COMP_HUFFMAN_DEFAULT)
				   && qat->interm_buff_mz == NULL
				   && im_bufs > 0))
			qat_xform->qat_comp_request_type =
					QAT_COMP_REQUEST_FIXED_COMP_STATELESS;

		else if ((xform->compress.deflate.huffman ==
				RTE_COMP_HUFFMAN_DYNAMIC ||
				xform->compress.deflate.huffman ==
						RTE_COMP_HUFFMAN_DEFAULT) &&
				(qat->interm_buff_mz != NULL ||
						im_bufs == 0))

			qat_xform->qat_comp_request_type =
					QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS;

		else {
			QAT_LOG(ERR,
					"IM buffers needed for dynamic deflate. Set size in config file");
			return -EINVAL;
		}

		qat_xform->checksum_type = xform->compress.chksum;

	} else {
		qat_xform->qat_comp_request_type = QAT_COMP_REQUEST_DECOMPRESS;
		qat_xform->checksum_type = xform->decompress.chksum;
	}

	if (qat_comp_create_templates(qat_xform, qat->interm_buff_mz, xform,
				      NULL, RTE_COMP_OP_STATELESS,
				      qat_dev_gen)) {
		QAT_LOG(ERR, "QAT: Problem with setting compression");
		return -EINVAL;
	}
	return 0;
}

/**
 * Free driver private_xform data.
 *
 * @param dev
 *   Compressdev device
 * @param private_xform
 *   handle of pmd's private_xform data
 * @return
 *  - 0 if successful
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 */
int
qat_comp_private_xform_free(struct rte_compressdev *dev __rte_unused,
			    void *private_xform)
{
	struct qat_comp_xform *qat_xform =
			(struct qat_comp_xform *)private_xform;

	if (qat_xform) {
		memset(qat_xform, 0, qat_comp_xform_size());
		struct rte_mempool *mp = rte_mempool_from_obj(qat_xform);

		rte_mempool_put(mp, qat_xform);
		return 0;
	}
	return -EINVAL;
}

/**
 * Reset stream state for the next use.
 *
 * @param stream
 *   handle of pmd's private stream data
 */
static void
qat_comp_stream_reset(struct qat_comp_stream *stream)
{
	if (stream) {
		memset(&stream->qat_xform, 0, sizeof(struct qat_comp_xform));
		stream->start_of_packet = 1;
		stream->op_in_progress = 0;
	}
}

/**
 * Create driver private stream data.
 *
 * @param dev
 *   Compressdev device
 * @param xform
 *   xform data
 * @param stream
 *   ptr where handle of pmd's private stream data should be stored
 * @return
 *  - Returns 0 if private stream structure has been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support STATEFUL operations.
 *  - Returns -ENOTSUP if comp device does not support the comp transform.
 *  - Returns -ENOMEM if the private stream could not be allocated.
 */
int
qat_comp_stream_create(struct rte_compressdev *dev,
		       const struct rte_comp_xform *xform,
		       void **stream)
{
	struct qat_comp_dev_private *qat = dev->data->dev_private;
	struct qat_comp_stream *ptr;

	if (unlikely(stream == NULL)) {
		QAT_LOG(ERR, "QAT: stream parameter is NULL");
		return -EINVAL;
	}
	if (unlikely(xform->type == RTE_COMP_COMPRESS)) {
		QAT_LOG(ERR, "QAT: stateful compression not supported");
		return -ENOTSUP;
	}
	if (unlikely(qat->streampool == NULL)) {
		QAT_LOG(ERR, "QAT device has no stream mempool");
		return -ENOMEM;
	}
	if (rte_mempool_get(qat->streampool, stream)) {
		QAT_LOG(ERR, "Couldn't get object from qat stream mempool");
		return -ENOMEM;
	}

	ptr = (struct qat_comp_stream *) *stream;
	qat_comp_stream_reset(ptr);
	ptr->qat_xform.qat_comp_request_type = QAT_COMP_REQUEST_DECOMPRESS;
	ptr->qat_xform.checksum_type = xform->decompress.chksum;

	if (qat_comp_create_templates(&ptr->qat_xform, qat->interm_buff_mz,
				      xform, ptr, RTE_COMP_OP_STATEFUL,
				      qat->qat_dev->qat_dev_gen)) {
		QAT_LOG(ERR, "QAT: problem with creating descriptor template for stream");
		rte_mempool_put(qat->streampool, *stream);
		*stream = NULL;
		return -EINVAL;
	}

	return 0;
}

/**
 * Free driver private stream data.
 *
 * @param dev
 *   Compressdev device
 * @param stream
 *   handle of pmd's private stream data
 * @return
 *  - 0 if successful
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support STATEFUL operations.
 *  - Returns -EBUSY if can't free stream as there are inflight operations
 */
int
qat_comp_stream_free(struct rte_compressdev *dev, void *stream)
{
	if (stream) {
		struct qat_comp_dev_private *qat = dev->data->dev_private;
		qat_comp_stream_reset((struct qat_comp_stream *) stream);
		rte_mempool_put(qat->streampool, stream);
		return 0;
	}
	return -EINVAL;
}

/**
 * Enqueue packets for processing on queue pair of a device
 *
 * @param qp
 *   qat queue pair
 * @param ops
 *   Compressdev operation
 * @param nb_ops
 *   number of operations
 * @return
 *  - nb_ops_sent if successful
 */
uint16_t
qat_enqueue_comp_op_burst(void *qp, void **ops, uint16_t nb_ops)
{
	register struct qat_queue *queue;
	struct qat_qp *tmp_qp = (struct qat_qp *)qp;
	register uint32_t nb_ops_sent = 0;
	register int nb_desc_to_build;
	uint16_t nb_ops_possible = nb_ops;
	register uint8_t *base_addr;
	register uint32_t tail;

	int descriptors_built, total_descriptors_built = 0;
	int nb_remaining_descriptors;
	int overflow = 0;

	if (unlikely(nb_ops == 0))
		return 0;

	/* read params used a lot in main loop into registers */
	queue = &(tmp_qp->tx_q);
	base_addr = (uint8_t *)queue->base_addr;
	tail = queue->tail;

	/* Find how many can actually fit on the ring */
	{
		/* dequeued can only be written by one thread, but it may not
		 * be this thread. As it's 4-byte aligned it will be read
		 * atomically here by any Intel CPU.
		 * enqueued can wrap before dequeued, but cannot
		 * lap it as var size of enq/deq (uint32_t) > var size of
		 * max_inflights (uint16_t). In reality inflights is never
		 * even as big as max uint16_t, as it's <= ADF_MAX_DESC.
		 * On wrapping, the calculation still returns the correct
		 * positive value as all three vars are unsigned.
		 */
		uint32_t inflights =
			tmp_qp->enqueued - tmp_qp->dequeued;

		/* Find how many can actually fit on the ring */
		overflow = (inflights + nb_ops) - tmp_qp->max_inflights;
		if (overflow > 0) {
			nb_ops_possible = nb_ops - overflow;
			if (nb_ops_possible == 0)
				return 0;
		}

		/* QAT has plenty of work queued already, so don't waste cycles
		 * enqueueing, wait til the application has gathered a bigger
		 * burst or some completed ops have been dequeued
		 */
		if (tmp_qp->min_enq_burst_threshold && inflights >
				QAT_QP_MIN_INFL_THRESHOLD && nb_ops_possible <
				tmp_qp->min_enq_burst_threshold) {
			tmp_qp->stats.threshold_hit_count++;
			return 0;
		}
	}

	/* At this point nb_ops_possible is assuming a 1:1 mapping
	 * between ops and descriptors.
	 * Fewer may be sent if some ops have to be split.
	 * nb_ops_possible is <= burst size.
	 * Find out how many spaces are actually available on the qp in case
	 * more are needed.
	 */
	nb_remaining_descriptors = nb_ops_possible
			 + ((overflow >= 0) ? 0 : overflow * (-1));
	QAT_DP_LOG(DEBUG, "Nb ops requested %d, nb descriptors remaining %d",
			nb_ops, nb_remaining_descriptors);

	while (nb_ops_sent != nb_ops_possible &&
				nb_remaining_descriptors > 0) {
		struct qat_comp_op_cookie *cookie =
				tmp_qp->op_cookies[tail >> queue->trailz];

		descriptors_built = 0;

		QAT_DP_LOG(DEBUG, "--- data length: %u",
			   ((struct rte_comp_op *)*ops)->src.length);

		nb_desc_to_build = qat_comp_build_request(*ops,
				base_addr + tail, cookie, tmp_qp->qat_dev_gen);
		QAT_DP_LOG(DEBUG, "%d descriptors built, %d remaining, "
			"%d ops sent, %d descriptors needed",
			total_descriptors_built, nb_remaining_descriptors,
			nb_ops_sent, nb_desc_to_build);

		if (unlikely(nb_desc_to_build < 0)) {
			/* this message cannot be enqueued */
			tmp_qp->stats.enqueue_err_count++;
			if (nb_ops_sent == 0)
				return 0;
			goto kick_tail;
		} else if (unlikely(nb_desc_to_build > 1)) {
			/* this op is too big and must be split - get more
			 * descriptors and retry
			 */

			QAT_DP_LOG(DEBUG, "Build %d descriptors for this op",
					nb_desc_to_build);

			nb_remaining_descriptors -= nb_desc_to_build;
			if (nb_remaining_descriptors >= 0) {
				/* There are enough remaining descriptors
				 * so retry
				 */
				int ret2 = qat_comp_build_multiple_requests(
						*ops, tmp_qp, tail,
						nb_desc_to_build);

				if (unlikely(ret2 < 1)) {
					QAT_DP_LOG(DEBUG,
							"Failed to build (%d) descriptors, status %d",
							nb_desc_to_build, ret2);

					qat_comp_free_split_op_memzones(cookie,
							nb_desc_to_build - 1);

					tmp_qp->stats.enqueue_err_count++;

					/* This message cannot be enqueued */
					if (nb_ops_sent == 0)
						return 0;
					goto kick_tail;
				} else {
					descriptors_built = ret2;
					total_descriptors_built +=
							descriptors_built;
					nb_remaining_descriptors -=
							descriptors_built;
					QAT_DP_LOG(DEBUG,
							"Multiple descriptors (%d) built ok",
							descriptors_built);
				}
			} else {
				QAT_DP_LOG(ERR, "For the current op, number of requested descriptors (%d) "
						"exceeds number of available descriptors (%d)",
						nb_desc_to_build,
						nb_remaining_descriptors +
							nb_desc_to_build);

				qat_comp_free_split_op_memzones(cookie,
						nb_desc_to_build - 1);

				/* Not enough extra descriptors */
				if (nb_ops_sent == 0)
					return 0;
				goto kick_tail;
			}
		} else {
			descriptors_built = 1;
			total_descriptors_built++;
			nb_remaining_descriptors--;
			QAT_DP_LOG(DEBUG, "Single descriptor built ok");
		}

		tail = adf_modulo(tail + (queue->msg_size * descriptors_built),
				  queue->modulo_mask);
		ops++;
		nb_ops_sent++;
	}

kick_tail:
	queue->tail = tail;
	tmp_qp->enqueued += total_descriptors_built;
	tmp_qp->stats.enqueued_count += nb_ops_sent;
	txq_write_tail(tmp_qp->qat_dev_gen, tmp_qp, queue);
	return nb_ops_sent;
}
