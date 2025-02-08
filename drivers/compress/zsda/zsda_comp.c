/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include "zsda_comp.h"

#define ZLIB_HEADER_SIZE 2
#define ZLIB_TRAILER_SIZE 4
#define GZIP_HEADER_SIZE 10
#define GZIP_TRAILER_SIZE 8
#define CHECKSUM_SIZE 4

int
zsda_comp_match(const void *op_in)
{
	const struct rte_comp_op *op = op_in;
	const struct zsda_comp_xform *xform = op->private_xform;

	if (op->op_type != RTE_COMP_OP_STATELESS)
		return 0;

	if (xform->type != RTE_COMP_COMPRESS)
		return 0;

	return 1;
}

int
zsda_decomp_match(const void *op_in)
{
	const struct rte_comp_op *op = op_in;
	const struct zsda_comp_xform *xform = op->private_xform;

	if (op->op_type != RTE_COMP_OP_STATELESS)
		return 0;

	if (xform->type != RTE_COMP_DECOMPRESS)
		return 0;
	return 1;
}

static uint8_t
zsda_opcode_get(const struct zsda_comp_xform *xform)
{
	if (xform->type == RTE_COMP_COMPRESS) {
		if (xform->checksum_type == RTE_COMP_CHECKSUM_NONE ||
			xform->checksum_type == RTE_COMP_CHECKSUM_CRC32)
			return ZSDA_OPC_COMP_GZIP;
		else if (xform->checksum_type == RTE_COMP_CHECKSUM_ADLER32)
			return ZSDA_OPC_COMP_ZLIB;
	}
	if (xform->type == RTE_COMP_DECOMPRESS) {
		if (xform->checksum_type == RTE_COMP_CHECKSUM_CRC32 ||
			xform->checksum_type == RTE_COMP_CHECKSUM_NONE)
			return ZSDA_OPC_DECOMP_GZIP;
		else if (xform->checksum_type == RTE_COMP_CHECKSUM_ADLER32)
			return ZSDA_OPC_DECOMP_ZLIB;
	}

	return ZSDA_OPC_INVALID;
}

int
zsda_comp_request_build(void *op_in, const struct zsda_queue *queue,
		   void **op_cookies, const uint16_t new_tail)
{
	struct rte_comp_op *op = op_in;
	struct zsda_comp_xform *xform = op->private_xform;
	struct zsda_wqe_comp *wqe =
		(struct zsda_wqe_comp *)(queue->base_addr +
					 (new_tail * queue->msg_size));

	struct zsda_op_cookie *cookie = op_cookies[new_tail];
	struct zsda_sgl *sgl_src = (struct zsda_sgl *)&cookie->sgl_src;
	struct zsda_sgl *sgl_dst = (struct zsda_sgl *)&cookie->sgl_dst;
	struct comp_head_info comp_head_info;

	uint8_t opcode;
	int ret;
	uint32_t op_offset;
	uint32_t op_src_len;
	uint32_t op_dst_len;
	uint32_t head_len;

	if ((op->m_dst == NULL) || (op->m_dst == op->m_src)) {
		ZSDA_LOG(ERR, "Failed! m_dst");
		return -EINVAL;
	}

	opcode = zsda_opcode_get(xform);
	if (opcode == ZSDA_OPC_INVALID) {
		ZSDA_LOG(ERR, "Failed! zsda_opcode_get");
		return -EINVAL;
	}

	cookie->used = true;
	cookie->sid = new_tail;
	cookie->op = op;

	if (opcode == ZSDA_OPC_COMP_GZIP)
		head_len = GZIP_HEADER_SIZE;
	else if (opcode == ZSDA_OPC_COMP_ZLIB)
		head_len = ZLIB_HEADER_SIZE;
	else {
		ZSDA_LOG(ERR, "Comp, op_code error!");
		return -EINVAL;
	}

	comp_head_info.head_len = head_len;
	comp_head_info.head_phys_addr = cookie->comp_head_phys_addr;

	op_offset = op->src.offset;
	op_src_len = op->src.length;
	ret = zsda_sgl_fill(op->m_src, op_offset, sgl_src,
				   cookie->sgl_src_phys_addr, op_src_len, NULL);

	op_offset = op->dst.offset;
	op_dst_len = op->m_dst->pkt_len - op_offset;
	op_dst_len += head_len;
	ret |= zsda_sgl_fill(op->m_dst, op_offset, sgl_dst,
					cookie->sgl_dst_phys_addr, op_dst_len,
					&comp_head_info);

	if (ret) {
		ZSDA_LOG(ERR, "Failed! zsda_sgl_fill");
		return ret;
	}

	memset(wqe, 0, sizeof(struct zsda_wqe_comp));
	wqe->rx_length = op_src_len;
	wqe->tx_length = op_dst_len;
	wqe->valid = queue->valid;
	wqe->op_code = opcode;
	wqe->sid = cookie->sid;
	wqe->rx_sgl_type = WQE_ELM_TYPE_LIST;
	wqe->tx_sgl_type = WQE_ELM_TYPE_LIST;

	wqe->rx_addr = cookie->sgl_src_phys_addr;
	wqe->tx_addr = cookie->sgl_dst_phys_addr;

	return ret;
}

int
zsda_decomp_request_build(void *op_in, const struct zsda_queue *queue,
			 void **op_cookies, const uint16_t new_tail)
{
	struct rte_comp_op *op = op_in;
	struct zsda_comp_xform *xform = op->private_xform;

	struct zsda_wqe_comp *wqe =
		(struct zsda_wqe_comp *)(queue->base_addr +
					 (new_tail * queue->msg_size));
	struct zsda_op_cookie *cookie = op_cookies[new_tail];
	struct zsda_sgl *sgl_src = (struct zsda_sgl *)&cookie->sgl_src;
	struct zsda_sgl *sgl_dst = (struct zsda_sgl *)&cookie->sgl_dst;
	uint8_t opcode;
	int ret;

	uint32_t op_offset;
	uint32_t op_src_len;
	uint32_t op_dst_len;

	uint8_t *head_data;
	uint16_t head_len;
	struct comp_head_info comp_head_info;
	uint8_t head_zlib[ZLIB_HEADER_SIZE] = {0x78, 0xDA};
	uint8_t head_gzip[GZIP_HEADER_SIZE] = {0x1F, 0x8B, 0x08, 0x00, 0x00,
						   0x00, 0x00, 0x00, 0x00, 0x03};

	if ((op->m_dst == NULL) || (op->m_dst == op->m_src)) {
		ZSDA_LOG(ERR, "Failed! m_dst");
		return -EINVAL;
	}

	opcode = zsda_opcode_get(xform);
	if (opcode == ZSDA_OPC_INVALID) {
		ZSDA_LOG(ERR, "Failed! zsda_opcode_get");
		return -EINVAL;
	}

	cookie->used = true;
	cookie->sid = new_tail;
	cookie->op = op;

	if (opcode == ZSDA_OPC_DECOMP_GZIP) {
		head_data = head_gzip;
		head_len = GZIP_HEADER_SIZE;
	} else if (opcode == ZSDA_OPC_DECOMP_ZLIB) {
		head_data = head_zlib;
		head_len = ZLIB_HEADER_SIZE;
	} else {
		ZSDA_LOG(ERR, "Comp, op_code error!");
		return -EINVAL;
	}

	op_offset = op->src.offset;
	op_src_len = op->src.length;
	op_src_len += head_len;
	comp_head_info.head_len = head_len;
	comp_head_info.head_phys_addr = cookie->comp_head_phys_addr;
	cookie->decomp_no_tail = true;
	for (int i = 0; i < head_len; i++)
		cookie->comp_head[i] = head_data[i];

	ret = zsda_sgl_fill(op->m_src, op_offset, sgl_src,
				cookie->sgl_src_phys_addr, op_src_len,
				&comp_head_info);

	op_offset = op->dst.offset;
	op_dst_len = op->m_dst->pkt_len - op_offset;
	ret |= zsda_sgl_fill(op->m_dst, op_offset, sgl_dst,
				 cookie->sgl_dst_phys_addr, op_dst_len, NULL);

	if (ret) {
		ZSDA_LOG(ERR, "Failed! zsda_sgl_fill");
		return ret;
	}

	memset(wqe, 0, sizeof(struct zsda_wqe_comp));

	wqe->rx_length = op_src_len;
	wqe->tx_length = op_dst_len;
	wqe->valid = queue->valid;
	wqe->op_code = opcode;
	wqe->sid = cookie->sid;
	wqe->rx_sgl_type = WQE_ELM_TYPE_LIST;
	wqe->tx_sgl_type = WQE_ELM_TYPE_LIST;
	wqe->rx_addr = cookie->sgl_src_phys_addr;
	wqe->tx_addr = cookie->sgl_dst_phys_addr;

	return ret;
}
