/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
#include <cryptodev_pmd.h>

#include "virtqueue.h"
#include "virtio_ring.h"
#include "virtio_cryptodev.h"
#include "virtio_crypto_algs.h"

static void
vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx)
{
	struct vring_desc *dp, *dp_tail;
	struct vq_desc_extra *dxp;
	uint16_t desc_idx_last = desc_idx;

	dp = &vq->vq_split.ring.desc[desc_idx];
	dxp = &vq->vq_descx[desc_idx];
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt + dxp->ndescs);
	if ((dp->flags & VRING_DESC_F_INDIRECT) == 0) {
		while (dp->flags & VRING_DESC_F_NEXT) {
			desc_idx_last = dp->next;
			dp = &vq->vq_split.ring.desc[dp->next];
		}
	}
	dxp->ndescs = 0;

	/*
	 * We must append the existing free chain, if any, to the end of
	 * newly freed chain. If the virtqueue was completely used, then
	 * head would be VQ_RING_DESC_CHAIN_END (ASSERTed above).
	 */
	if (vq->vq_desc_tail_idx == VQ_RING_DESC_CHAIN_END) {
		vq->vq_desc_head_idx = desc_idx;
	} else {
		dp_tail = &vq->vq_split.ring.desc[vq->vq_desc_tail_idx];
		dp_tail->next = desc_idx;
	}

	vq->vq_desc_tail_idx = desc_idx_last;
	dp->next = VQ_RING_DESC_CHAIN_END;
}

static uint16_t
virtqueue_dequeue_burst_rx(struct virtqueue *vq,
		struct rte_crypto_op **rx_pkts, uint16_t num)
{
	struct vring_used_elem *uep;
	struct rte_crypto_op *cop;
	uint16_t used_idx, desc_idx;
	uint16_t i;
	struct virtio_crypto_inhdr *inhdr;
	struct virtio_crypto_op_cookie *op_cookie;

	/* Caller does the check */
	for (i = 0; i < num ; i++) {
		used_idx = (uint16_t)(vq->vq_used_cons_idx
				& (vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];
		desc_idx = (uint16_t)uep->id;
		cop = (struct rte_crypto_op *)
				vq->vq_descx[desc_idx].crypto_op;
		if (unlikely(cop == NULL)) {
			VIRTIO_CRYPTO_RX_LOG_DBG("vring descriptor with no "
					"mbuf cookie at %u",
					vq->vq_used_cons_idx);
			break;
		}

		op_cookie = (struct virtio_crypto_op_cookie *)
						vq->vq_descx[desc_idx].cookie;
		inhdr = &(op_cookie->inhdr);
		switch (inhdr->status) {
		case VIRTIO_CRYPTO_OK:
			cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			break;
		case VIRTIO_CRYPTO_ERR:
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			vq->packets_received_failed++;
			break;
		case VIRTIO_CRYPTO_BADMSG:
			cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			vq->packets_received_failed++;
			break;
		case VIRTIO_CRYPTO_NOTSUPP:
			cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			vq->packets_received_failed++;
			break;
		case VIRTIO_CRYPTO_INVSESS:
			cop->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			vq->packets_received_failed++;
			break;
		default:
			break;
		}

		vq->packets_received_total++;

		rx_pkts[i] = cop;
		rte_mempool_put(vq->mpool, op_cookie);

		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);
		vq->vq_descx[desc_idx].crypto_op = NULL;
	}

	return i;
}

static uint16_t
virtqueue_dequeue_burst_rx_packed(struct virtqueue *vq,
		struct rte_crypto_op **rx_pkts, uint16_t num)
{
	struct rte_crypto_op *cop;
	uint16_t used_idx;
	uint16_t i;
	struct virtio_crypto_inhdr *inhdr;
	struct virtio_crypto_op_cookie *op_cookie;
	struct vring_packed_desc *desc;

	desc = vq->vq_packed.ring.desc;

	/* Caller does the check */
	for (i = 0; i < num ; i++) {
		used_idx = vq->vq_used_cons_idx;
		if (!desc_is_used(&desc[used_idx], vq))
			break;

		cop = (struct rte_crypto_op *)
				vq->vq_descx[used_idx].crypto_op;
		if (unlikely(cop == NULL)) {
			VIRTIO_CRYPTO_RX_LOG_DBG("vring descriptor with no "
					"mbuf cookie at %u",
					vq->vq_used_cons_idx);
			break;
		}

		op_cookie = (struct virtio_crypto_op_cookie *)
						vq->vq_descx[used_idx].cookie;
		inhdr = &(op_cookie->inhdr);
		switch (inhdr->status) {
		case VIRTIO_CRYPTO_OK:
			cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			break;
		case VIRTIO_CRYPTO_ERR:
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			vq->packets_received_failed++;
			break;
		case VIRTIO_CRYPTO_BADMSG:
			cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			vq->packets_received_failed++;
			break;
		case VIRTIO_CRYPTO_NOTSUPP:
			cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			vq->packets_received_failed++;
			break;
		case VIRTIO_CRYPTO_INVSESS:
			cop->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;
			vq->packets_received_failed++;
			break;
		default:
			break;
		}

		vq->packets_received_total++;

		if (cop->asym->rsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN)
			memcpy(cop->asym->rsa.sign.data, op_cookie->sign,
					cop->asym->rsa.sign.length);
		else if (cop->asym->rsa.op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
			memcpy(cop->asym->rsa.message.data, op_cookie->message,
					cop->asym->rsa.message.length);
		else if (cop->asym->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT)
			memcpy(cop->asym->rsa.cipher.data, op_cookie->cipher,
					cop->asym->rsa.cipher.length);
		else if (cop->asym->rsa.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT)
			memcpy(cop->asym->rsa.message.data, op_cookie->message,
					cop->asym->rsa.message.length);

		rx_pkts[i] = cop;
		rte_mempool_put(vq->mpool, op_cookie);

		vq->vq_free_cnt += 4;
		vq->vq_used_cons_idx += 4;
		vq->vq_descx[used_idx].crypto_op = NULL;
		if (vq->vq_used_cons_idx >= vq->vq_nentries) {
			vq->vq_used_cons_idx -= vq->vq_nentries;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
	}

	return i;
}

static __rte_always_inline uint8_t
virtqueue_crypto_check_cipher_request(struct virtio_crypto_cipher_data_req *req)
{
	if (likely((req->para.iv_len <= VIRTIO_CRYPTO_MAX_IV_SIZE) &&
		(req->para.src_data_len <= RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.dst_data_len >= req->para.src_data_len) &&
		(req->para.dst_data_len <= RTE_MBUF_DEFAULT_BUF_SIZE)))
		return VIRTIO_CRYPTO_OK;
	return VIRTIO_CRYPTO_BADMSG;
}

static __rte_always_inline uint8_t
virtqueue_crypto_check_chain_request(struct virtio_crypto_alg_chain_data_req *req)
{
	if (likely((req->para.iv_len <= VIRTIO_CRYPTO_MAX_IV_SIZE) &&
		(req->para.src_data_len <= RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.dst_data_len >= req->para.src_data_len) &&
		(req->para.dst_data_len <= RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.cipher_start_src_offset <
			RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.len_to_cipher <= RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.hash_start_src_offset <
			RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.len_to_hash <= RTE_MBUF_DEFAULT_BUF_SIZE) &&
		(req->para.cipher_start_src_offset + req->para.len_to_cipher <=
			req->para.src_data_len) &&
		(req->para.hash_start_src_offset + req->para.len_to_hash <=
			req->para.src_data_len) &&
		(req->para.dst_data_len + req->para.hash_result_len <=
			RTE_MBUF_DEFAULT_BUF_SIZE)))
		return VIRTIO_CRYPTO_OK;
	return VIRTIO_CRYPTO_BADMSG;
}

static inline int
virtqueue_crypto_sym_pkt_header_arrange(
		struct rte_crypto_op *cop,
		struct virtio_crypto_op_data_req *data,
		struct virtio_crypto_session *session)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct virtio_crypto_op_data_req *req_data = data;
	struct virtio_crypto_op_ctrl_req *ctrl = &session->ctrl.hdr;
	struct virtio_crypto_sym_create_session_req *sym_sess_req =
		&ctrl->u.sym_create_session;
	struct virtio_crypto_alg_chain_session_para *chain_para =
		&sym_sess_req->u.chain.para;
	struct virtio_crypto_cipher_session_para *cipher_para;

	req_data->header.session_id = session->session_id;

	switch (sym_sess_req->op_type) {
	case VIRTIO_CRYPTO_SYM_OP_CIPHER:
		req_data->u.sym_req.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;

		cipher_para = &sym_sess_req->u.cipher.para;
		if (cipher_para->op == VIRTIO_CRYPTO_OP_ENCRYPT)
			req_data->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
		else
			req_data->header.opcode = VIRTIO_CRYPTO_CIPHER_DECRYPT;

		req_data->u.sym_req.u.cipher.para.iv_len
			= session->iv.length;

		req_data->u.sym_req.u.cipher.para.src_data_len =
			(sym_op->cipher.data.length +
				sym_op->cipher.data.offset);
		req_data->u.sym_req.u.cipher.para.dst_data_len =
			req_data->u.sym_req.u.cipher.para.src_data_len;
		if (virtqueue_crypto_check_cipher_request(
			&req_data->u.sym_req.u.cipher) != VIRTIO_CRYPTO_OK)
			return -1;
		break;
	case VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING:
		req_data->u.sym_req.op_type =
			VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING;

		cipher_para = &chain_para->cipher_param;
		if (cipher_para->op == VIRTIO_CRYPTO_OP_ENCRYPT)
			req_data->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
		else
			req_data->header.opcode = VIRTIO_CRYPTO_CIPHER_DECRYPT;

		req_data->u.sym_req.u.chain.para.iv_len = session->iv.length;
		req_data->u.sym_req.u.chain.para.aad_len = session->aad.length;

		req_data->u.sym_req.u.chain.para.src_data_len =
			RTE_MAX((sym_op->cipher.data.length +
				 sym_op->cipher.data.offset),
				(sym_op->auth.data.length +
				 sym_op->auth.data.offset));
		req_data->u.sym_req.u.chain.para.dst_data_len =
			req_data->u.sym_req.u.chain.para.src_data_len;
		req_data->u.sym_req.u.chain.para.cipher_start_src_offset =
			sym_op->cipher.data.offset;
		req_data->u.sym_req.u.chain.para.len_to_cipher =
			sym_op->cipher.data.length;
		req_data->u.sym_req.u.chain.para.hash_start_src_offset =
			sym_op->auth.data.offset;
		req_data->u.sym_req.u.chain.para.len_to_hash =
			sym_op->auth.data.length;
		req_data->u.sym_req.u.chain.para.aad_len =
			chain_para->aad_len;

		if (chain_para->hash_mode == VIRTIO_CRYPTO_SYM_HASH_MODE_PLAIN)
			req_data->u.sym_req.u.chain.para.hash_result_len =
				chain_para->u.hash_param.hash_result_len;
		if (chain_para->hash_mode ==
			VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH)
			req_data->u.sym_req.u.chain.para.hash_result_len =
				chain_para->u.mac_param.hash_result_len;
		if (virtqueue_crypto_check_chain_request(
			&req_data->u.sym_req.u.chain) != VIRTIO_CRYPTO_OK)
			return -1;
		break;
	default:
		return -1;
	}

	return 0;
}

static inline int
virtqueue_crypto_sym_enqueue_xmit_split(
		struct virtqueue *txvq,
		struct rte_crypto_op *cop)
{
	uint16_t idx = 0;
	uint16_t num_entry;
	uint16_t needed = 1;
	uint16_t head_idx;
	struct vq_desc_extra *dxp;
	struct vring_desc *start_dp;
	struct vring_desc *desc;
	uint64_t indirect_op_data_req_phys_addr;
	uint16_t req_data_len = sizeof(struct virtio_crypto_op_data_req);
	uint32_t indirect_vring_addr_offset = req_data_len +
		sizeof(struct virtio_crypto_inhdr);
	uint32_t indirect_iv_addr_offset =
			offsetof(struct virtio_crypto_op_cookie, iv);
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct virtio_crypto_session *session =
		CRYPTODEV_GET_SYM_SESS_PRIV(cop->sym->session);
	struct virtio_crypto_op_data_req *op_data_req;
	uint32_t hash_result_len = 0;
	struct virtio_crypto_op_cookie *crypto_op_cookie;
	struct virtio_crypto_alg_chain_session_para *para;
	uint32_t src_len;

	if (unlikely(sym_op->m_src->nb_segs != 1))
		return -EMSGSIZE;
	if (unlikely(txvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(txvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	head_idx = txvq->vq_desc_head_idx;
	if (unlikely(head_idx >= txvq->vq_nentries))
		return -EFAULT;
	if (unlikely(session == NULL))
		return -EFAULT;

	dxp = &txvq->vq_descx[head_idx];

	if (rte_mempool_get(txvq->mpool, &dxp->cookie)) {
		VIRTIO_CRYPTO_TX_LOG_ERR("can not get cookie");
		return -EFAULT;
	}
	crypto_op_cookie = dxp->cookie;
	indirect_op_data_req_phys_addr =
		rte_mempool_virt2iova(crypto_op_cookie);
	op_data_req = (struct virtio_crypto_op_data_req *)crypto_op_cookie;

	if (virtqueue_crypto_sym_pkt_header_arrange(cop, op_data_req, session))
		return -EFAULT;

	/* status is initialized to VIRTIO_CRYPTO_ERR */
	((struct virtio_crypto_inhdr *)
		((uint8_t *)op_data_req + req_data_len))->status =
		VIRTIO_CRYPTO_ERR;

	/* point to indirect vring entry */
	desc = (struct vring_desc *)
		((uint8_t *)op_data_req + indirect_vring_addr_offset);
	for (idx = 0; idx < (NUM_ENTRY_VIRTIO_CRYPTO_OP - 1); idx++)
		desc[idx].next = idx + 1;
	desc[NUM_ENTRY_VIRTIO_CRYPTO_OP - 1].next = VQ_RING_DESC_CHAIN_END;

	idx = 0;

	/* indirect vring: first part, virtio_crypto_op_data_req */
	desc[idx].addr = indirect_op_data_req_phys_addr;
	desc[idx].len = req_data_len;
	desc[idx++].flags = VRING_DESC_F_NEXT;

	/* indirect vring: iv of cipher */
	if (session->iv.length) {
		if (cop->phys_addr)
			desc[idx].addr = cop->phys_addr + session->iv.offset;
		else {
			if (session->iv.length > VIRTIO_CRYPTO_MAX_IV_SIZE)
				return -ENOMEM;

			rte_memcpy(crypto_op_cookie->iv,
					rte_crypto_op_ctod_offset(cop,
					uint8_t *, session->iv.offset),
					session->iv.length);
			desc[idx].addr = indirect_op_data_req_phys_addr +
				indirect_iv_addr_offset;
		}

		desc[idx].len = session->iv.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;
	}

	/* indirect vring: additional auth data */
	if (session->aad.length) {
		desc[idx].addr = session->aad.phys_addr;
		desc[idx].len = session->aad.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;
	}

	src_len = RTE_MAX((sym_op->cipher.data.offset +
			   sym_op->cipher.data.length),
			  (sym_op->auth.data.length +
			   sym_op->auth.data.offset));
	/* indirect vring: src data */
	desc[idx].addr = rte_pktmbuf_iova_offset(sym_op->m_src, 0);
	desc[idx].len = src_len;
	desc[idx++].flags = VRING_DESC_F_NEXT;

	/* indirect vring: dst data */
	if (sym_op->m_dst) {
		desc[idx].addr = rte_pktmbuf_iova_offset(sym_op->m_dst, 0);
		desc[idx].len = src_len;
	} else {
		desc[idx].addr = rte_pktmbuf_iova_offset(sym_op->m_src, 0);
		desc[idx].len = src_len;
	}
	desc[idx++].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;

	/* indirect vring: digest result */
	para = &(session->ctrl.hdr.u.sym_create_session.u.chain.para);
	if (para->hash_mode == VIRTIO_CRYPTO_SYM_HASH_MODE_PLAIN)
		hash_result_len = para->u.hash_param.hash_result_len;
	if (para->hash_mode == VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH)
		hash_result_len = para->u.mac_param.hash_result_len;
	if (hash_result_len > 0) {
		desc[idx].addr = sym_op->auth.digest.phys_addr;
		desc[idx].len = hash_result_len;
		desc[idx++].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
	}

	/* indirect vring: last part, status returned */
	desc[idx].addr = indirect_op_data_req_phys_addr + req_data_len;
	desc[idx].len = sizeof(struct virtio_crypto_inhdr);
	desc[idx++].flags = VRING_DESC_F_WRITE;

	num_entry = idx;

	/* save the infos to use when receiving packets */
	dxp->crypto_op = (void *)cop;
	dxp->ndescs = needed;

	/* use a single buffer */
	start_dp = txvq->vq_split.ring.desc;
	start_dp[head_idx].addr = indirect_op_data_req_phys_addr +
		indirect_vring_addr_offset;
	start_dp[head_idx].len = num_entry * sizeof(struct vring_desc);
	start_dp[head_idx].flags = VRING_DESC_F_INDIRECT;

	idx = start_dp[head_idx].next;
	txvq->vq_desc_head_idx = idx;
	if (txvq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		txvq->vq_desc_tail_idx = idx;
	txvq->vq_free_cnt = (uint16_t)(txvq->vq_free_cnt - needed);
	vq_update_avail_ring(txvq, head_idx);

	return 0;
}

static inline int
virtqueue_crypto_sym_enqueue_xmit_packed(
		struct virtqueue *txvq,
		struct rte_crypto_op *cop)
{
	uint16_t idx = 0;
	uint16_t needed = 1;
	uint16_t head_idx;
	struct vq_desc_extra *dxp;
	struct vring_packed_desc *start_dp;
	struct vring_packed_desc *desc;
	uint64_t op_data_req_phys_addr;
	uint16_t req_data_len = sizeof(struct virtio_crypto_op_data_req);
	uint32_t iv_addr_offset =
			offsetof(struct virtio_crypto_op_cookie, iv);
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct virtio_crypto_session *session =
		CRYPTODEV_GET_SYM_SESS_PRIV(cop->sym->session);
	struct virtio_crypto_op_data_req *op_data_req;
	uint32_t hash_result_len = 0;
	struct virtio_crypto_op_cookie *crypto_op_cookie;
	struct virtio_crypto_alg_chain_session_para *para;
	uint16_t flags = VRING_DESC_F_NEXT;

	if (unlikely(sym_op->m_src->nb_segs != 1))
		return -EMSGSIZE;
	if (unlikely(txvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(txvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	head_idx = txvq->vq_desc_head_idx;
	if (unlikely(head_idx >= txvq->vq_nentries))
		return -EFAULT;
	if (unlikely(session == NULL))
		return -EFAULT;

	dxp = &txvq->vq_descx[head_idx];

	if (rte_mempool_get(txvq->mpool, &dxp->cookie)) {
		VIRTIO_CRYPTO_TX_LOG_ERR("can not get cookie");
		return -EFAULT;
	}
	crypto_op_cookie = dxp->cookie;
	op_data_req_phys_addr = rte_mempool_virt2iova(crypto_op_cookie);
	op_data_req = (struct virtio_crypto_op_data_req *)crypto_op_cookie;

	if (virtqueue_crypto_sym_pkt_header_arrange(cop, op_data_req, session))
		return -EFAULT;

	/* status is initialized to VIRTIO_CRYPTO_ERR */
	((struct virtio_crypto_inhdr *)
		((uint8_t *)op_data_req + req_data_len))->status =
		VIRTIO_CRYPTO_ERR;

	desc = &txvq->vq_packed.ring.desc[txvq->vq_desc_head_idx];
	needed = 4;
	flags |= txvq->vq_packed.cached_flags;

	start_dp = desc;
	idx = 0;

	/* packed vring: first part, virtio_crypto_op_data_req */
	desc[idx].addr = op_data_req_phys_addr;
	desc[idx].len = req_data_len;
	desc[idx++].flags = flags;

	/* packed vring: iv of cipher */
	if (session->iv.length) {
		if (cop->phys_addr)
			desc[idx].addr = cop->phys_addr + session->iv.offset;
		else {
			if (session->iv.length > VIRTIO_CRYPTO_MAX_IV_SIZE)
				return -ENOMEM;

			rte_memcpy(crypto_op_cookie->iv,
					rte_crypto_op_ctod_offset(cop,
					uint8_t *, session->iv.offset),
					session->iv.length);
			desc[idx].addr = op_data_req_phys_addr + iv_addr_offset;
		}

		desc[idx].len = session->iv.length;
		desc[idx++].flags = flags;
	}

	/* packed vring: additional auth data */
	if (session->aad.length) {
		desc[idx].addr = session->aad.phys_addr;
		desc[idx].len = session->aad.length;
		desc[idx++].flags = flags;
	}

	/* packed vring: src data */
	desc[idx].addr = rte_pktmbuf_iova_offset(sym_op->m_src, 0);
	desc[idx].len = (sym_op->cipher.data.offset
		+ sym_op->cipher.data.length);
	desc[idx++].flags = flags;

	/* packed vring: dst data */
	if (sym_op->m_dst) {
		desc[idx].addr = rte_pktmbuf_iova_offset(sym_op->m_dst, 0);
		desc[idx].len = (sym_op->cipher.data.offset
			+ sym_op->cipher.data.length);
	} else {
		desc[idx].addr = rte_pktmbuf_iova_offset(sym_op->m_src, 0);
		desc[idx].len = (sym_op->cipher.data.offset
			+ sym_op->cipher.data.length);
	}
	desc[idx++].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;

	/* packed vring: digest result */
	para = &(session->ctrl.hdr.u.sym_create_session.u.chain.para);
	if (para->hash_mode == VIRTIO_CRYPTO_SYM_HASH_MODE_PLAIN)
		hash_result_len = para->u.hash_param.hash_result_len;
	if (para->hash_mode == VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH)
		hash_result_len = para->u.mac_param.hash_result_len;
	if (hash_result_len > 0) {
		desc[idx].addr = sym_op->auth.digest.phys_addr;
		desc[idx].len = hash_result_len;
		desc[idx++].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
	}

	/* packed vring: last part, status returned */
	desc[idx].addr = op_data_req_phys_addr + req_data_len;
	desc[idx].len = sizeof(struct virtio_crypto_inhdr);
	desc[idx++].flags = flags | VRING_DESC_F_WRITE;

	/* save the infos to use when receiving packets */
	dxp->crypto_op = (void *)cop;
	dxp->ndescs = needed;

	txvq->vq_desc_head_idx += idx & (txvq->vq_nentries - 1);
	if (txvq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		txvq->vq_desc_tail_idx = idx;
	txvq->vq_free_cnt = (uint16_t)(txvq->vq_free_cnt - needed);
	virtqueue_store_flags_packed(&start_dp[0],
					start_dp[0].flags | flags,
				    txvq->hw->weak_barriers);
	virtio_wmb(txvq->hw->weak_barriers);

	return 0;
}

static inline int
virtqueue_crypto_sym_enqueue_xmit(
		struct virtqueue *txvq,
		struct rte_crypto_op *cop)
{
	if (vtpci_with_packed_queue(txvq->hw))
		return virtqueue_crypto_sym_enqueue_xmit_packed(txvq, cop);
	else
		return virtqueue_crypto_sym_enqueue_xmit_split(txvq, cop);
}

static inline int
virtqueue_crypto_asym_pkt_header_arrange(
		struct rte_crypto_op *cop,
		struct virtio_crypto_op_data_req *data,
		struct virtio_crypto_session *session)
{
	struct virtio_crypto_op_ctrl_req *ctrl = &session->ctrl.hdr;
	struct virtio_crypto_op_data_req *req_data = data;
	struct rte_crypto_asym_op *asym_op = cop->asym;

	req_data->header.session_id = session->session_id;

	switch (ctrl->header.algo)	{
	case VIRTIO_CRYPTO_AKCIPHER_RSA:
		req_data->header.algo = ctrl->header.algo;
		if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
			req_data->header.opcode = VIRTIO_CRYPTO_AKCIPHER_SIGN;
			req_data->u.akcipher_req.para.src_data_len
				= asym_op->rsa.message.length;
			req_data->u.akcipher_req.para.dst_data_len
				= asym_op->rsa.sign.length;
		} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
			req_data->header.opcode = VIRTIO_CRYPTO_AKCIPHER_VERIFY;
			req_data->u.akcipher_req.para.src_data_len
				= asym_op->rsa.sign.length;
			req_data->u.akcipher_req.para.dst_data_len
				= asym_op->rsa.message.length;
		} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			req_data->header.opcode = VIRTIO_CRYPTO_AKCIPHER_ENCRYPT;
			req_data->u.akcipher_req.para.src_data_len
				= asym_op->rsa.message.length;
			req_data->u.akcipher_req.para.dst_data_len
				= asym_op->rsa.cipher.length;
		} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			req_data->header.opcode = VIRTIO_CRYPTO_AKCIPHER_DECRYPT;
			req_data->u.akcipher_req.para.src_data_len
				= asym_op->rsa.cipher.length;
			req_data->u.akcipher_req.para.dst_data_len
				= asym_op->rsa.message.length;
		} else {
			return -EINVAL;
		}

		break;
	default:
		req_data->header.algo = VIRTIO_CRYPTO_NO_AKCIPHER;
	}

	return 0;
}

static inline int
virtqueue_crypto_asym_enqueue_xmit_split(
		struct virtqueue *txvq,
		struct rte_crypto_op *cop)
{
	uint16_t req_data_len = sizeof(struct virtio_crypto_op_data_req);
	uint32_t indirect_vring_addr_offset = req_data_len +
		sizeof(struct virtio_crypto_inhdr);
	struct virtio_crypto_session *session =
		CRYPTODEV_GET_ASYM_SESS_PRIV(cop->asym->session);
	struct virtio_crypto_op_cookie *crypto_op_cookie;
	struct rte_crypto_asym_op *asym_op = cop->asym;
	struct virtio_crypto_op_data_req *op_data_req;
	uint64_t indirect_op_data_req_phys_addr;
	struct vring_used_elem *uep;
	struct vring_desc *start_dp;
	struct vq_desc_extra *dxp;
	struct vring_desc *desc;
	uint16_t needed = 1;
	uint16_t num_entry;
	uint16_t head_idx;
	uint16_t idx = 0;

	if (unlikely(txvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(txvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	head_idx = txvq->vq_desc_head_idx;
	if (unlikely(head_idx >= txvq->vq_nentries))
		return -EFAULT;

	dxp = &txvq->vq_descx[head_idx];

	if (rte_mempool_get(txvq->mpool, &dxp->cookie)) {
		VIRTIO_CRYPTO_TX_LOG_ERR("can not get cookie");
		return -EFAULT;
	}
	crypto_op_cookie = dxp->cookie;
	indirect_op_data_req_phys_addr =
		rte_mempool_virt2iova(crypto_op_cookie);
	op_data_req = (struct virtio_crypto_op_data_req *)crypto_op_cookie;
	if (virtqueue_crypto_asym_pkt_header_arrange(cop, op_data_req, session))
		return -EFAULT;

	/* status is initialized to VIRTIO_CRYPTO_ERR */
	((struct virtio_crypto_inhdr *)
		((uint8_t *)op_data_req + req_data_len))->status =
		VIRTIO_CRYPTO_ERR;

	/* point to indirect vring entry */
	desc = (struct vring_desc *)
		((uint8_t *)op_data_req + indirect_vring_addr_offset);
	for (idx = 0; idx < (NUM_ENTRY_VIRTIO_CRYPTO_OP - 1); idx++)
		desc[idx].next = idx + 1;
	desc[NUM_ENTRY_VIRTIO_CRYPTO_OP - 1].next = VQ_RING_DESC_CHAIN_END;

	idx = 0;

	/* indirect vring: first part, virtio_crypto_op_data_req */
	desc[idx].addr = indirect_op_data_req_phys_addr;
	desc[idx].len = req_data_len;
	desc[idx++].flags = VRING_DESC_F_NEXT;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
		/* indirect vring: src data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.message.data);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;

		/* indirect vring: dst data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.sign.data);
		desc[idx].len = asym_op->rsa.sign.length;
		desc[idx++].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
	} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
		/* indirect vring: src data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.sign.data);
		desc[idx].len = asym_op->rsa.sign.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;

		/* indirect vring: dst data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.message.data);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;
	} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
		/* indirect vring: src data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.message.data);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;

		/* indirect vring: dst data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.cipher.data);
		desc[idx].len = asym_op->rsa.cipher.length;
		desc[idx++].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
	} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
		/* indirect vring: src data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.cipher.data);
		desc[idx].len = asym_op->rsa.cipher.length;
		desc[idx++].flags = VRING_DESC_F_NEXT;

		/* indirect vring: dst data */
		desc[idx].addr = rte_mem_virt2iova(asym_op->rsa.message.data);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
	} else {
		VIRTIO_CRYPTO_TX_LOG_ERR("Invalid asym op");
		return -EINVAL;
	}

	/* indirect vring: last part, status returned */
	desc[idx].addr = indirect_op_data_req_phys_addr + req_data_len;
	desc[idx].len = sizeof(struct virtio_crypto_inhdr);
	desc[idx++].flags = VRING_DESC_F_WRITE;

	num_entry = idx;

	/* save the infos to use when receiving packets */
	dxp->crypto_op = (void *)cop;
	dxp->ndescs = needed;

	/* use a single buffer */
	start_dp = txvq->vq_split.ring.desc;
	start_dp[head_idx].addr = indirect_op_data_req_phys_addr +
		indirect_vring_addr_offset;
	start_dp[head_idx].len = num_entry * sizeof(struct vring_desc);
	start_dp[head_idx].flags = VRING_DESC_F_INDIRECT;

	idx = start_dp[head_idx].next;
	txvq->vq_desc_head_idx = idx;
	if (txvq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		txvq->vq_desc_tail_idx = idx;
	txvq->vq_free_cnt = (uint16_t)(txvq->vq_free_cnt - needed);

	uep = &txvq->vq_split.ring.used->ring[head_idx];
	uep->id = head_idx;
	vq_update_avail_ring(txvq, head_idx);

	return 0;
}

static inline int
virtqueue_crypto_asym_enqueue_xmit_packed(
		struct virtqueue *txvq,
		struct rte_crypto_op *cop)
{
	uint16_t idx = 0;
	uint16_t num_entry;
	uint16_t needed = 1;
	uint16_t head_idx;
	struct vq_desc_extra *dxp;
	struct vring_packed_desc *start_dp;
	struct vring_packed_desc *desc;
	uint64_t op_data_req_phys_addr;
	uint16_t req_data_len = sizeof(struct virtio_crypto_op_data_req);
	struct rte_crypto_asym_op *asym_op = cop->asym;
	struct virtio_crypto_session *session =
		CRYPTODEV_GET_ASYM_SESS_PRIV(cop->asym->session);
	struct virtio_crypto_op_data_req *op_data_req;
	struct virtio_crypto_op_cookie *crypto_op_cookie;
	uint16_t flags = VRING_DESC_F_NEXT;

	if (unlikely(txvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(txvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	head_idx = txvq->vq_desc_head_idx;
	if (unlikely(head_idx >= txvq->vq_nentries))
		return -EFAULT;

	dxp = &txvq->vq_descx[head_idx];

	if (rte_mempool_get(txvq->mpool, &dxp->cookie)) {
		VIRTIO_CRYPTO_TX_LOG_ERR("can not get cookie");
		return -EFAULT;
	}
	crypto_op_cookie = dxp->cookie;
	op_data_req_phys_addr =	rte_mempool_virt2iova(crypto_op_cookie);
	op_data_req = (struct virtio_crypto_op_data_req *)crypto_op_cookie;
	if (virtqueue_crypto_asym_pkt_header_arrange(cop, op_data_req, session))
		return -EFAULT;

	/* status is initialized to VIRTIO_CRYPTO_ERR */
	((struct virtio_crypto_inhdr *)
		((uint8_t *)op_data_req + req_data_len))->status =
		VIRTIO_CRYPTO_ERR;

	desc = &txvq->vq_packed.ring.desc[txvq->vq_desc_head_idx];
	needed = 4;
	flags |= txvq->vq_packed.cached_flags;

	start_dp = desc;
	idx = 0;

	/* packed vring: first part, virtio_crypto_op_data_req */
	desc[idx].addr = op_data_req_phys_addr;
	desc[idx].len = sizeof(struct virtio_crypto_op_data_req);
	desc[idx++].flags = flags;

	if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
		/* packed vring: src data */
		if (asym_op->rsa.message.length > VIRTIO_CRYPTO_MAX_MSG_SIZE)
			return -ENOMEM;
		memcpy(crypto_op_cookie->message, asym_op->rsa.message.data,
				asym_op->rsa.message.length);
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, message);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = flags;

		/* packed vring: dst data */
		if (asym_op->rsa.sign.length > VIRTIO_CRYPTO_MAX_SIGN_SIZE)
			return -ENOMEM;
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, sign);
		desc[idx].len = asym_op->rsa.sign.length;
		desc[idx++].flags = flags | VRING_DESC_F_WRITE;
	} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
		/* packed vring: src data */
		if (asym_op->rsa.sign.length > VIRTIO_CRYPTO_MAX_SIGN_SIZE)
			return -ENOMEM;
		memcpy(crypto_op_cookie->sign, asym_op->rsa.sign.data,
				asym_op->rsa.sign.length);
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, sign);
		desc[idx].len = asym_op->rsa.sign.length;
		desc[idx++].flags = flags;

		/* packed vring: dst data */
		if (asym_op->rsa.message.length > VIRTIO_CRYPTO_MAX_MSG_SIZE)
			return -ENOMEM;
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, message);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = flags;
	} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
		/* packed vring: src data */
		if (asym_op->rsa.message.length > VIRTIO_CRYPTO_MAX_MSG_SIZE)
			return -ENOMEM;
		memcpy(crypto_op_cookie->message, asym_op->rsa.message.data,
				asym_op->rsa.message.length);
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, message);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = flags;

		/* packed vring: dst data */
		if (asym_op->rsa.cipher.length > VIRTIO_CRYPTO_MAX_CIPHER_SIZE)
			return -ENOMEM;
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, cipher);
		desc[idx].len = asym_op->rsa.cipher.length;
		desc[idx++].flags = flags | VRING_DESC_F_WRITE;
	} else if (asym_op->rsa.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
		/* packed vring: src data */
		if (asym_op->rsa.cipher.length > VIRTIO_CRYPTO_MAX_CIPHER_SIZE)
			return -ENOMEM;
		memcpy(crypto_op_cookie->cipher, asym_op->rsa.cipher.data,
				asym_op->rsa.cipher.length);
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, cipher);
		desc[idx].len = asym_op->rsa.cipher.length;
		desc[idx++].flags = flags;

		/* packed vring: dst data */
		if (asym_op->rsa.message.length > VIRTIO_CRYPTO_MAX_MSG_SIZE)
			return -ENOMEM;
		desc[idx].addr = op_data_req_phys_addr +
			offsetof(struct virtio_crypto_op_cookie, message);
		desc[idx].len = asym_op->rsa.message.length;
		desc[idx++].flags = flags | VRING_DESC_F_WRITE;
	} else {
		VIRTIO_CRYPTO_TX_LOG_ERR("Invalid asym op");
		return -EINVAL;
	}

	/* packed vring: last part, status returned */
	desc[idx].addr = op_data_req_phys_addr + req_data_len;
	desc[idx].len = sizeof(struct virtio_crypto_inhdr);
	desc[idx++].flags = txvq->vq_packed.cached_flags | VRING_DESC_F_WRITE;

	num_entry = idx;
	txvq->vq_avail_idx += num_entry;
	if (txvq->vq_avail_idx >= txvq->vq_nentries) {
		txvq->vq_avail_idx -= txvq->vq_nentries;
		txvq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	/* save the infos to use when receiving packets */
	dxp->crypto_op = (void *)cop;
	dxp->ndescs = needed;

	txvq->vq_desc_head_idx = (txvq->vq_desc_head_idx + idx) & (txvq->vq_nentries - 1);
	if (txvq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		txvq->vq_desc_tail_idx = idx;
	txvq->vq_free_cnt = (uint16_t)(txvq->vq_free_cnt - needed);
	virtqueue_store_flags_packed(&start_dp[0],
					start_dp[0].flags | flags,
				    txvq->hw->weak_barriers);
	virtio_wmb(txvq->hw->weak_barriers);
	return 0;
}

static inline int
virtqueue_crypto_asym_enqueue_xmit(
		struct virtqueue *txvq,
		struct rte_crypto_op *cop)
{
	if (vtpci_with_packed_queue(txvq->hw))
		return virtqueue_crypto_asym_enqueue_xmit_packed(txvq, cop);
	else
		return virtqueue_crypto_asym_enqueue_xmit_split(txvq, cop);
}

static int
virtio_crypto_vring_start(struct virtqueue *vq)
{
	struct virtio_crypto_hw *hw = vq->hw;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	PMD_INIT_FUNC_TRACE();

	if (ring_mem == NULL) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("virtqueue ring memory is NULL");
		return -EINVAL;
	}

	/*
	 * Set guest physical address of the virtqueue
	 * in VIRTIO_PCI_QUEUE_PFN config register of device
	 * to share with the backend
	 */
	if (VTPCI_OPS(hw)->setup_queue(hw, vq) < 0) {
		VIRTIO_CRYPTO_INIT_LOG_ERR("setup_queue failed");
		return -EINVAL;
	}

	return 0;
}

void
virtio_crypto_ctrlq_start(struct rte_cryptodev *dev)
{
	struct virtio_crypto_hw *hw = dev->data->dev_private;

	if (hw->cvq) {
		rte_spinlock_init(&hw->cvq->lock);
		virtio_crypto_vring_start(virtcrypto_cq_to_vq(hw->cvq));
		VIRTQUEUE_DUMP(virtcrypto_cq_to_vq(hw->cvq));
	}
}

void
virtio_crypto_dataq_start(struct rte_cryptodev *dev)
{
	/*
	 * Start data vrings
	 * -	Setup vring structure for data queues
	 */
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	/* Start data vring. */
	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		virtio_crypto_vring_start(dev->data->queue_pairs[i]);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->queue_pairs[i]);
	}
}

/* vring size of data queue is 1024 */
#define VIRTIO_MBUF_BURST_SZ 1024

uint16_t
virtio_crypto_pkt_rx_burst(void *tx_queue, struct rte_crypto_op **rx_pkts,
		uint16_t nb_pkts)
{
	struct virtqueue *txvq = tx_queue;
	uint16_t nb_used, num, nb_rx;

	virtio_rmb(0);

	num = (uint16_t)(likely(nb_pkts <= VIRTIO_MBUF_BURST_SZ)
		? nb_pkts : VIRTIO_MBUF_BURST_SZ);
	if (num == 0)
		return 0;

	if (likely(vtpci_with_packed_queue(txvq->hw))) {
		nb_rx = virtqueue_dequeue_burst_rx_packed(txvq, rx_pkts, num);
	} else {
		nb_used = VIRTQUEUE_NUSED(txvq);
		num = (uint16_t)(likely(num <= nb_used) ? num : nb_used);
		nb_rx = virtqueue_dequeue_burst_rx(txvq, rx_pkts, num);
	}

	VIRTIO_CRYPTO_RX_LOG_DBG("used:%d dequeue:%d", nb_rx, num);

	return nb_rx;
}

uint16_t
virtio_crypto_pkt_tx_burst(void *tx_queue, struct rte_crypto_op **tx_pkts,
		uint16_t nb_pkts)
{
	struct virtqueue *txvq;
	uint16_t nb_tx;
	int error;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;
	if (unlikely(tx_queue == NULL)) {
		VIRTIO_CRYPTO_TX_LOG_ERR("tx_queue is NULL");
		return 0;
	}
	txvq = tx_queue;

	VIRTIO_CRYPTO_TX_LOG_DBG("%d packets to xmit", nb_pkts);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		if (tx_pkts[nb_tx]->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			struct rte_mbuf *txm = tx_pkts[nb_tx]->sym->m_src;
			/* nb_segs is always 1 at virtio crypto situation */
			int need = txm->nb_segs - txvq->vq_free_cnt;

			/*
			 * Positive value indicates it hasn't enough space in vring
			 * descriptors
			 */
			if (unlikely(need > 0)) {
				/*
				 * try it again because the receive process may be
				 * free some space
				 */
				need = txm->nb_segs - txvq->vq_free_cnt;
				if (unlikely(need > 0)) {
					VIRTIO_CRYPTO_TX_LOG_DBG("No free tx "
											 "descriptors to transmit");
					break;
				}
			}

			/* Enqueue Packet buffers */
			error = virtqueue_crypto_sym_enqueue_xmit(txvq, tx_pkts[nb_tx]);
		} else if (tx_pkts[nb_tx]->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
			/* Enqueue Packet buffers */
			error = virtqueue_crypto_asym_enqueue_xmit(txvq, tx_pkts[nb_tx]);
		} else {
			VIRTIO_CRYPTO_TX_LOG_ERR("invalid crypto op type %u",
				tx_pkts[nb_tx]->type);
			txvq->packets_sent_failed++;
			continue;
		}

		if (unlikely(error)) {
			if (error == ENOSPC)
				VIRTIO_CRYPTO_TX_LOG_ERR(
					"virtqueue_enqueue Free count = 0");
			else if (error == EMSGSIZE)
				VIRTIO_CRYPTO_TX_LOG_ERR(
					"virtqueue_enqueue Free count < 1");
			else
				VIRTIO_CRYPTO_TX_LOG_ERR(
					"virtqueue_enqueue error: %d", error);
			txvq->packets_sent_failed++;
			break;
		}

		txvq->packets_sent_total++;
	}

	if (likely(nb_tx)) {
		if (vtpci_with_packed_queue(txvq->hw)) {
			virtqueue_notify(txvq);
			VIRTIO_CRYPTO_TX_LOG_DBG("Notified backend after xmit");
			return nb_tx;
		}

		vq_update_avail_idx(txvq);

		if (unlikely(virtqueue_kick_prepare(txvq))) {
			virtqueue_notify(txvq);
			VIRTIO_CRYPTO_TX_LOG_DBG("Notified backend after xmit");
		}
	}

	return nb_tx;
}
