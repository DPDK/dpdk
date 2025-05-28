/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include "zsda_comp.h"

#define ZLIB_HEADER_SIZE 2
#define ZLIB_TRAILER_SIZE 4
#define GZIP_HEADER_SIZE 10
#define GZIP_TRAILER_SIZE 8
#define CHECKSUM_SIZE 4

#define POLYNOMIAL 0xEDB88320
static uint32_t crc32_table[8][256];
static int table_config;

static void
crc32_table_build(void)
{
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t crc = i;
		for (uint32_t j = 0; j < 8; j++)
			crc = (crc >> 1) ^ ((crc & 1) ? POLYNOMIAL : 0);
		crc32_table[0][i] = crc;
	}

	for (int i = 1; i < 8; i++) {
		for (uint32_t j = 0; j < 256; j++)
			crc32_table[i][j] = (crc32_table[i-1][j] >> 8) ^
					crc32_table[0][crc32_table[i-1][j] & 0xFF];
	}
	table_config = 1;
}

static uint32_t
zsda_crc32(const uint8_t *data, size_t length)
{
	uint32_t crc = 0xFFFFFFFF;

	if (!table_config)
		crc32_table_build();

	while (length >= 8) {
		crc ^= *(const uint32_t *)data;
		crc = crc32_table[7][crc & 0xFF] ^
			  crc32_table[6][(crc >> 8) & 0xFF] ^
			  crc32_table[5][(crc >> 16) & 0xFF] ^
			  crc32_table[4][(crc >> 24) & 0xFF] ^
			  crc32_table[3][data[4]] ^
			  crc32_table[2][data[5]] ^
			  crc32_table[1][data[6]] ^
			  crc32_table[0][data[7]];

		data += 8;
		length -= 8;
	}

	for (size_t i = 0; i < length; i++)
		crc = (crc >> 8) ^ crc32_table[0][(crc ^ data[i]) & 0xFF];

	return crc ^ 0xFFFFFFFF;
}

#define MOD_ADLER 65521
#define NMAX 5552
static uint32_t
zsda_adler32(const uint8_t *buf, uint32_t len)
{
	uint32_t s1 = 1;
	uint32_t s2 = 0;

	while (len > 0) {
		uint32_t k = (len < NMAX) ? len : NMAX;
		len -= k;

		for (uint32_t i = 0; i < k; i++) {
			s1 += buf[i];
			s2 += s1;
		}

		s1 %= MOD_ADLER;
		s2 %= MOD_ADLER;

		buf += k;
	}

	return (s2 << 16) | s1;
}

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
zsda_comp_wqe_build(void *op_in, const struct zsda_queue *queue,
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

static uint32_t
zsda_chksum_read(uint8_t *data_addr, uint8_t op_code, uint32_t produced)
{
	uint8_t *chk_addr;
	uint32_t chksum = 0;
	int i = 0;

	if (op_code == ZSDA_OPC_COMP_ZLIB) {
		chk_addr = data_addr + produced - ZLIB_TRAILER_SIZE;
		for (i = 0; i < CHECKSUM_SIZE; i++) {
			chksum = chksum << 8;
			chksum |= (*(chk_addr + i));
		}
	} else if (op_code == ZSDA_OPC_COMP_GZIP) {
		chk_addr = data_addr + produced - GZIP_TRAILER_SIZE;
		for (i = 0; i < CHECKSUM_SIZE; i++)
			chksum |= (*(chk_addr + i) << (i * 8));
	}

	return chksum;
}

int
zsda_comp_callback(void *cookie_in, struct zsda_cqe *cqe)
{
	struct zsda_op_cookie *tmp_cookie = cookie_in;
	struct rte_comp_op *tmp_op = tmp_cookie->op;
	uint8_t *data_addr =
		(uint8_t *)tmp_op->m_dst->buf_addr + tmp_op->m_dst->data_off;
	uint32_t chksum = 0;
	uint16_t head_len;
	uint16_t tail_len;

	if (tmp_cookie->decomp_no_tail && CQE_ERR0_RIGHT(cqe->err0))
		cqe->err0 = 0x0000;

	if (!(CQE_ERR0(cqe->err0) || CQE_ERR1(cqe->err1)))
		tmp_op->status = RTE_COMP_OP_STATUS_SUCCESS;
	else {
		tmp_op->status = RTE_COMP_OP_STATUS_ERROR;
		return ZSDA_FAILED;
	}

	/* handle chksum */
	tmp_op->produced = cqe->tx_real_length;
	if (cqe->op_code == ZSDA_OPC_COMP_ZLIB) {
		head_len = ZLIB_HEADER_SIZE;
		tail_len = ZLIB_TRAILER_SIZE;
		chksum = zsda_chksum_read(data_addr, cqe->op_code,
						  tmp_op->produced - head_len);
	}
	if (cqe->op_code == ZSDA_OPC_COMP_GZIP) {
		head_len = GZIP_HEADER_SIZE;
		tail_len = GZIP_TRAILER_SIZE;
		chksum = zsda_chksum_read(data_addr, cqe->op_code,
						  tmp_op->produced - head_len);
	} else if (cqe->op_code == ZSDA_OPC_DECOMP_ZLIB) {
		head_len = ZLIB_HEADER_SIZE;
		tail_len = ZLIB_TRAILER_SIZE;
		chksum = zsda_adler32(data_addr, tmp_op->produced);
	} else if (cqe->op_code == ZSDA_OPC_DECOMP_GZIP) {
		head_len = GZIP_HEADER_SIZE;
		tail_len = GZIP_TRAILER_SIZE;
		chksum = zsda_crc32(data_addr, tmp_op->produced);
	}
	tmp_op->output_chksum = chksum;

	if (cqe->op_code == ZSDA_OPC_COMP_ZLIB ||
		cqe->op_code == ZSDA_OPC_COMP_GZIP) {
		/* remove tail data*/
		rte_pktmbuf_trim(tmp_op->m_dst, GZIP_TRAILER_SIZE);
		/* remove head and tail length */
		tmp_op->produced = tmp_op->produced - (head_len + tail_len);
	}

	return ZSDA_SUCCESS;
}
