/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#include "ena_eth_com.h"

struct ena_eth_io_rx_cdesc_ext *ena_com_get_next_rx_cdesc(struct ena_com_io_cq *io_cq)
{
	struct ena_eth_io_rx_cdesc_ext *cdesc;
	u16 expected_phase, head_masked;
	u16 desc_phase;

	head_masked = io_cq->head & (io_cq->q_depth - 1);
	expected_phase = io_cq->phase;

	cdesc = (struct ena_eth_io_rx_cdesc_ext *)(io_cq->cdesc_addr.virt_addr
			+ (head_masked * io_cq->cdesc_entry_size_in_bytes));

	desc_phase = ENA_FIELD_GET(READ_ONCE32(cdesc->base.status),
				   ENA_ETH_IO_RX_CDESC_BASE_PHASE_MASK,
				   ENA_ETH_IO_RX_CDESC_BASE_PHASE_SHIFT);

	if (desc_phase != expected_phase)
		return NULL;

	/* Make sure we read the rest of the descriptor after the phase bit
	 * has been read
	 */
	dma_rmb();

	return cdesc;
}

void ena_com_dump_single_rx_cdesc(struct ena_com_io_cq *io_cq,
				  struct ena_eth_io_rx_cdesc_ext *desc)
{
	if (desc) {
		uint32_t *desc_arr = (uint32_t *)desc;

		ena_trc_err(ena_com_io_cq_to_ena_dev(io_cq),
				"RX descriptor value[0x%08x 0x%08x 0x%08x 0x%08x] phase[%u] first[%u] last[%u] MBZ7[%u] MBZ17[%u]\n",
				desc_arr[0], desc_arr[1], desc_arr[2], desc_arr[3],
				ENA_FIELD_GET(desc->base.status,
					(uint32_t)ENA_ETH_IO_RX_CDESC_BASE_PHASE_MASK,
					ENA_ETH_IO_RX_CDESC_BASE_PHASE_SHIFT),
				ENA_FIELD_GET(desc->base.status,
					(uint32_t)ENA_ETH_IO_RX_CDESC_BASE_FIRST_MASK,
					ENA_ETH_IO_RX_CDESC_BASE_FIRST_SHIFT),
				ENA_FIELD_GET(desc->base.status,
					(uint32_t)ENA_ETH_IO_RX_CDESC_BASE_LAST_MASK,
					ENA_ETH_IO_RX_CDESC_BASE_LAST_SHIFT),
				ENA_FIELD_GET(desc->base.status,
					(uint32_t)ENA_ETH_IO_RX_CDESC_BASE_MBZ7_MASK,
					ENA_ETH_IO_RX_CDESC_BASE_MBZ7_SHIFT),
				ENA_FIELD_GET(desc->base.status,
					(uint32_t)ENA_ETH_IO_RX_CDESC_BASE_MBZ17_MASK,
					ENA_ETH_IO_RX_CDESC_BASE_MBZ17_SHIFT));
		if (unlikely(ena_com_is_extended_rx_cdesc(io_cq)))
			ena_trc_err(ena_com_io_cq_to_ena_dev(io_cq),
				    "RX descriptor timestamp %" ENA_PRIu64 "(nsec)",
				    (uint64_t)desc->timestamp_low |
				    (uint64_t)desc->timestamp_high << 32);
	}
}

void ena_com_dump_single_tx_cdesc(struct ena_com_io_cq *io_cq,
				  struct ena_eth_io_tx_cdesc_ext *desc)
{
	if (desc) {
		uint32_t *desc_arr = (uint32_t *)desc;

		ena_trc_err(ena_com_io_cq_to_ena_dev(io_cq),
			    "TX descriptor value[0x%08x 0x%08x] phase[%u] MBZ6[%u]\n",
			    desc_arr[0], desc_arr[1],
			    ENA_FIELD_GET(desc->base.flags,
				(uint32_t)ENA_ETH_IO_TX_CDESC_PHASE_MASK,
				0),
			    ENA_FIELD_GET(desc->base.flags,
				(uint32_t)ENA_ETH_IO_TX_CDESC_MBZ6_MASK,
				ENA_ETH_IO_TX_CDESC_MBZ6_SHIFT));

		if (unlikely(ena_com_is_extended_tx_cdesc(io_cq)))
			ena_trc_err(ena_com_io_cq_to_ena_dev(io_cq),
				    "TX descriptor timestamp %" ENA_PRIu64 "(nsec)",
				    (uint64_t)desc->timestamp_low |
				    (uint64_t)desc->timestamp_high << 32);
	}
}

struct ena_eth_io_tx_cdesc_ext *ena_com_tx_cdesc_idx_to_ptr(struct ena_com_io_cq *io_cq, u16 idx)
{
	idx &= (io_cq->q_depth - 1);

	return (struct ena_eth_io_tx_cdesc_ext *)
		((uintptr_t)io_cq->cdesc_addr.virt_addr +
		idx * io_cq->cdesc_entry_size_in_bytes);
}

static void *get_sq_desc_regular_queue(struct ena_com_io_sq *io_sq)
{
	u16 tail_masked;
	u32 offset;

	tail_masked = io_sq->tail & (io_sq->q_depth - 1);

	offset = tail_masked * io_sq->desc_entry_size;

	return (void *)((uintptr_t)io_sq->desc_addr.virt_addr + offset);
}

static int ena_com_write_bounce_buffer_to_dev(struct ena_com_io_sq *io_sq,
						     u8 *bounce_buffer)
{
	struct ena_com_llq_info *llq_info = &io_sq->llq_info;
	u16 dst_tail_mask;
	u32 dst_offset;

	dst_tail_mask = io_sq->tail & (io_sq->q_depth - 1);
	dst_offset = dst_tail_mask * llq_info->desc_list_entry_size;

	if (is_llq_max_tx_burst_exists(io_sq)) {
		if (unlikely(!io_sq->entries_in_tx_burst_left)) {
			ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
				    "Error: trying to send more packets than tx burst allows\n");
			return ENA_COM_NO_SPACE;
		}

		io_sq->entries_in_tx_burst_left--;
		ena_trc_dbg(ena_com_io_sq_to_ena_dev(io_sq),
			    "Decreasing entries_in_tx_burst_left of queue %u to %u\n",
			    io_sq->qid, io_sq->entries_in_tx_burst_left);
	}

	/* Make sure everything was written into the bounce buffer before
	 * writing the bounce buffer to the device
	 */
	wmb();

	/* The line is completed. Copy it to dev */
	ENA_MEMCPY_TO_DEVICE_64(io_sq->desc_addr.pbuf_dev_addr + dst_offset,
				bounce_buffer,
				llq_info->desc_list_entry_size);

	io_sq->tail++;

	/* Switch phase bit in case of wrap around */
	if (unlikely((io_sq->tail & (io_sq->q_depth - 1)) == 0))
		io_sq->phase ^= 1;

	return ENA_COM_OK;
}

static int ena_com_write_header_to_bounce(struct ena_com_io_sq *io_sq,
						 u8 *header_src,
						 u16 header_len)
{
	struct ena_com_llq_pkt_ctrl *pkt_ctrl = &io_sq->llq_buf_ctrl;
	struct ena_com_llq_info *llq_info = &io_sq->llq_info;
	u8 *bounce_buffer = pkt_ctrl->curr_bounce_buf;
	u16 header_offset;

	if (unlikely(io_sq->mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST))
		return 0;

	header_offset =
		llq_info->descs_num_before_header * io_sq->desc_entry_size;

	if (unlikely((header_offset + header_len) >  llq_info->desc_list_entry_size)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Trying to write header larger than llq entry can accommodate\n");
		return ENA_COM_FAULT;
	}

	if (unlikely(!bounce_buffer)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Bounce buffer is NULL\n");
		return ENA_COM_FAULT;
	}

	memcpy(bounce_buffer + header_offset, header_src, header_len);

	return 0;
}

static void *get_sq_desc_llq(struct ena_com_io_sq *io_sq)
{
	struct ena_com_llq_pkt_ctrl *pkt_ctrl = &io_sq->llq_buf_ctrl;
	u8 *bounce_buffer;
	void *sq_desc;

	bounce_buffer = pkt_ctrl->curr_bounce_buf;

	if (unlikely(!bounce_buffer)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Bounce buffer is NULL\n");
		return NULL;
	}

	sq_desc = bounce_buffer + pkt_ctrl->idx * io_sq->desc_entry_size;
	pkt_ctrl->idx++;
	pkt_ctrl->descs_left_in_line--;

	return sq_desc;
}

static int ena_com_close_bounce_buffer(struct ena_com_io_sq *io_sq)
{
	struct ena_com_llq_pkt_ctrl *pkt_ctrl = &io_sq->llq_buf_ctrl;
	struct ena_com_llq_info *llq_info = &io_sq->llq_info;
	int rc;

	if (unlikely(io_sq->mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST))
		return ENA_COM_OK;

	/* bounce buffer was used, so write it and get a new one */
	if (likely(pkt_ctrl->idx)) {
		rc = ena_com_write_bounce_buffer_to_dev(io_sq,
							pkt_ctrl->curr_bounce_buf);
		if (unlikely(rc)) {
			ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
				    "Failed to write bounce buffer to device\n");
			return rc;
		}

		pkt_ctrl->curr_bounce_buf =
			ena_com_get_next_bounce_buffer(&io_sq->bounce_buf_ctrl);
		memset(io_sq->llq_buf_ctrl.curr_bounce_buf,
		       0x0, llq_info->desc_list_entry_size);
	}

	pkt_ctrl->idx = 0;
	pkt_ctrl->descs_left_in_line = llq_info->descs_num_before_header;
	return ENA_COM_OK;
}

/* There is an assumption in function get_tx_sq_desc() that both descriptors have the same size */
_Static_assert(sizeof(struct ena_eth_io_tx_meta_desc) == (sizeof(struct ena_eth_io_tx_desc)),
	       "ena_eth_io_tx_meta_desc should be the same size as ena_eth_io_tx_desc");
static void *get_tx_sq_desc(struct ena_com_io_sq *io_sq)
{
	void *tx_desc;

	if (io_sq->mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV)
		return get_sq_desc_llq(io_sq);

	tx_desc = get_sq_desc_regular_queue(io_sq);

	memset(tx_desc, 0x0, sizeof(struct ena_eth_io_tx_desc));

	return tx_desc;
}

static int ena_com_sq_update_llq_tail(struct ena_com_io_sq *io_sq)
{
	struct ena_com_llq_pkt_ctrl *pkt_ctrl = &io_sq->llq_buf_ctrl;
	struct ena_com_llq_info *llq_info = &io_sq->llq_info;
	int rc;

	if (!pkt_ctrl->descs_left_in_line) {
		rc = ena_com_write_bounce_buffer_to_dev(io_sq,
							pkt_ctrl->curr_bounce_buf);
		if (unlikely(rc)) {
			ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
				    "Failed to write bounce buffer to device\n");
			return rc;
		}

		pkt_ctrl->curr_bounce_buf =
			ena_com_get_next_bounce_buffer(&io_sq->bounce_buf_ctrl);
		memset(io_sq->llq_buf_ctrl.curr_bounce_buf,
		       0x0, llq_info->desc_list_entry_size);

		pkt_ctrl->idx = 0;
		pkt_ctrl->descs_left_in_line = llq_info->descs_per_entry;
	}

	return ENA_COM_OK;
}

static int ena_com_sq_update_reqular_queue_tail(struct ena_com_io_sq *io_sq)
{
	io_sq->tail++;

	/* Switch phase bit in case of wrap around */
	if (unlikely((io_sq->tail & (io_sq->q_depth - 1)) == 0))
		io_sq->phase ^= 1;

	return ENA_COM_OK;
}

static int ena_com_sq_update_tail(struct ena_com_io_sq *io_sq)
{
	if (io_sq->mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV)
		return ena_com_sq_update_llq_tail(io_sq);

	return ena_com_sq_update_reqular_queue_tail(io_sq);
}

struct ena_eth_io_rx_cdesc_ext *
	ena_com_rx_cdesc_idx_to_ptr(struct ena_com_io_cq *io_cq, u16 idx)
{
	idx &= (io_cq->q_depth - 1);
	return (struct ena_eth_io_rx_cdesc_ext *)
		((uintptr_t)io_cq->cdesc_addr.virt_addr +
		idx * io_cq->cdesc_entry_size_in_bytes);
}

static int ena_com_cdesc_rx_pkt_get(struct ena_com_io_cq *io_cq,
				    u16 *first_cdesc_idx,
				    u16 *num_descs)
{
	struct ena_com_dev *dev = ena_com_io_cq_to_ena_dev(io_cq);
	u16 count = io_cq->cur_rx_pkt_cdesc_count, head_masked;
	struct ena_eth_io_rx_cdesc_ext *cdesc;
	u32 last = 0;

	do {
		u32 status;

		cdesc = ena_com_get_next_rx_cdesc(io_cq);
		if (!cdesc)
			break;
		status = READ_ONCE32(cdesc->base.status);

		if (unlikely(ENA_FIELD_GET(status,
					   ENA_ETH_IO_RX_CDESC_BASE_FIRST_MASK,
					   ENA_ETH_IO_RX_CDESC_BASE_FIRST_SHIFT) &&
			     count != 0)) {
			ena_trc_err(dev,
				    "First bit is on in descriptor #%u on q_id: %u, req_id: %u\n",
				    count, io_cq->qid, cdesc->base.req_id);
			return ENA_COM_FAULT;
		}

		if (unlikely((status & (ENA_ETH_IO_RX_CDESC_BASE_MBZ7_MASK |
					ENA_ETH_IO_RX_CDESC_BASE_MBZ17_MASK)) &&
			      ena_com_get_cap(dev, ENA_ADMIN_CDESC_MBZ))) {
			ena_trc_err(dev,
				    "Corrupted RX descriptor #%u on q_id: %u, req_id: %u\n",
				    count, io_cq->qid, cdesc->base.req_id);
			return ENA_COM_FAULT;
		}

		if (unlikely(cdesc->base.req_id >= io_cq->q_depth)) {
			ena_trc_err(dev,
				    "Bad req_id in descriptor #%u on q_id: %u, req_id: %u\n",
				    count, io_cq->qid, cdesc->base.req_id);
			return ENA_COM_EIO;
		}

		ena_com_cq_inc_head(io_cq);
		count++;
		last = ENA_FIELD_GET(status,
				     ENA_ETH_IO_RX_CDESC_BASE_LAST_MASK,
				     ENA_ETH_IO_RX_CDESC_BASE_LAST_SHIFT);
	} while (!last);

	if (last) {
		*first_cdesc_idx = io_cq->cur_rx_pkt_cdesc_start_idx;

		head_masked = io_cq->head & (io_cq->q_depth - 1);

		*num_descs = count;
		io_cq->cur_rx_pkt_cdesc_count = 0;
		io_cq->cur_rx_pkt_cdesc_start_idx = head_masked;

		ena_trc_dbg(ena_com_io_cq_to_ena_dev(io_cq),
			    "ENA q_id: %u packets were completed. first desc idx %u descs# %u\n",
			    io_cq->qid, *first_cdesc_idx, count);
	} else {
		io_cq->cur_rx_pkt_cdesc_count = count;
		*num_descs = 0;
	}

	return ENA_COM_OK;
}

static int ena_com_create_meta(struct ena_com_io_sq *io_sq,
			       struct ena_com_tx_meta *ena_meta)
{
	struct ena_eth_io_tx_meta_desc *meta_desc = NULL;

	meta_desc = get_tx_sq_desc(io_sq);
	if (unlikely(!meta_desc))
		return ENA_COM_FAULT;

	meta_desc->len_ctrl |= ENA_ETH_IO_TX_META_DESC_META_DESC_MASK;

	meta_desc->len_ctrl |= ENA_ETH_IO_TX_META_DESC_EXT_VALID_MASK;

	/* bits 0-9 of the mss */
	meta_desc->word2 |=
		ENA_FIELD_PREP((u32)ena_meta->mss,
			       ENA_ETH_IO_TX_META_DESC_MSS_LO_MASK,
			       ENA_ETH_IO_TX_META_DESC_MSS_LO_SHIFT);
	/* bits 10-13 of the mss */
	meta_desc->len_ctrl |=
		ENA_FIELD_PREP((ena_meta->mss >> 10),
			       ENA_ETH_IO_TX_META_DESC_MSS_HI_MASK,
			       ENA_ETH_IO_TX_META_DESC_MSS_HI_SHIFT);

	/* Extended meta desc */
	meta_desc->len_ctrl |= ENA_ETH_IO_TX_META_DESC_ETH_META_TYPE_MASK;
	meta_desc->len_ctrl |=
		ENA_FIELD_PREP((u32)io_sq->phase,
			       ENA_ETH_IO_TX_META_DESC_PHASE_MASK,
			       ENA_ETH_IO_TX_META_DESC_PHASE_SHIFT);

	meta_desc->len_ctrl |= ENA_ETH_IO_TX_META_DESC_FIRST_MASK;
	meta_desc->len_ctrl |= ENA_ETH_IO_TX_META_DESC_META_STORE_MASK;

	meta_desc->word2 |= ena_meta->l3_hdr_len &
		ENA_ETH_IO_TX_META_DESC_L3_HDR_LEN_MASK;
	meta_desc->word2 |=
		ENA_FIELD_PREP(ena_meta->l3_hdr_offset,
			       ENA_ETH_IO_TX_META_DESC_L3_HDR_OFF_MASK,
			       ENA_ETH_IO_TX_META_DESC_L3_HDR_OFF_SHIFT);

	meta_desc->word2 |=
		ENA_FIELD_PREP((u32)ena_meta->l4_hdr_len,
			       ENA_ETH_IO_TX_META_DESC_L4_HDR_LEN_IN_WORDS_MASK,
			       ENA_ETH_IO_TX_META_DESC_L4_HDR_LEN_IN_WORDS_SHIFT);

	return ena_com_sq_update_tail(io_sq);
}

static int ena_com_create_and_store_tx_meta_desc(struct ena_com_io_sq *io_sq,
						 struct ena_com_tx_ctx *ena_tx_ctx,
						 bool *have_meta)
{
	struct ena_com_tx_meta *ena_meta = &ena_tx_ctx->ena_meta;

	/* When disable meta caching is set, don't bother to save the meta and
	 * compare it to the stored version, just create the meta
	 */
	if (io_sq->disable_meta_caching) {
		*have_meta = true;
		return ena_com_create_meta(io_sq, ena_meta);
	}

	if (ena_com_meta_desc_changed(io_sq, ena_tx_ctx)) {
		*have_meta = true;
		/* Cache the meta desc */
		memcpy(&io_sq->cached_tx_meta, ena_meta,
		       sizeof(struct ena_com_tx_meta));
		return ena_com_create_meta(io_sq, ena_meta);
	}

	*have_meta = false;
	return ENA_COM_OK;
}

static void ena_com_rx_set_flags(struct ena_com_rx_ctx *ena_rx_ctx,
				 struct ena_eth_io_rx_cdesc_ext *cdesc)
{
	ena_rx_ctx->l3_proto = cdesc->base.status &
		ENA_ETH_IO_RX_CDESC_BASE_L3_PROTO_IDX_MASK;
	ena_rx_ctx->l4_proto =
		ENA_FIELD_GET(cdesc->base.status,
			      ENA_ETH_IO_RX_CDESC_BASE_L4_PROTO_IDX_MASK,
			      ENA_ETH_IO_RX_CDESC_BASE_L4_PROTO_IDX_SHIFT);
	ena_rx_ctx->l3_csum_err =
		!!(ENA_FIELD_GET(cdesc->base.status,
				 ENA_ETH_IO_RX_CDESC_BASE_L3_CSUM_ERR_MASK,
				 ENA_ETH_IO_RX_CDESC_BASE_L3_CSUM_ERR_SHIFT));
	ena_rx_ctx->l4_csum_err =
		!!(ENA_FIELD_GET(cdesc->base.status,
				 ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_ERR_MASK,
				 ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_ERR_SHIFT));
	ena_rx_ctx->l4_csum_checked =
		!!(ENA_FIELD_GET(cdesc->base.status,
				 ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_CHECKED_MASK,
				 ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_CHECKED_SHIFT));
	ena_rx_ctx->hash = cdesc->base.hash;
	ena_rx_ctx->frag =
		ENA_FIELD_GET(cdesc->base.status,
			      ENA_ETH_IO_RX_CDESC_BASE_IPV4_FRAG_MASK,
			      ENA_ETH_IO_RX_CDESC_BASE_IPV4_FRAG_SHIFT);
}

/*****************************************************************************/
/*****************************     API      **********************************/
/*****************************************************************************/

int ena_com_prepare_tx(struct ena_com_io_sq *io_sq,
		       struct ena_com_tx_ctx *ena_tx_ctx,
		       int *nb_hw_desc)
{
	struct ena_com_buf *ena_bufs = ena_tx_ctx->ena_bufs;
	void *buffer_to_push = ena_tx_ctx->push_header;
	u16 header_len = ena_tx_ctx->header_len;
	struct ena_eth_io_tx_desc *desc = NULL;
	u16 num_bufs = ena_tx_ctx->num_bufs;
	u16 start_tail = io_sq->tail;
	bool have_meta;
	u64 addr_hi;
	int i, rc;

	ENA_WARN(io_sq->direction != ENA_COM_IO_QUEUE_DIRECTION_TX,
		 ena_com_io_sq_to_ena_dev(io_sq), "wrong Q type");

	/* num_bufs +1 for potential meta desc */
	if (unlikely(!ena_com_sq_have_enough_space(io_sq, num_bufs + 1))) {
		ena_trc_dbg(ena_com_io_sq_to_ena_dev(io_sq),
			    "Not enough space in the tx queue\n");
		return ENA_COM_NO_MEM;
	}

	if (unlikely(header_len > io_sq->tx_max_header_size)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Header size is too large %u max header: %u\n",
			    header_len, io_sq->tx_max_header_size);
		return ENA_COM_INVAL;
	}

	if (unlikely(io_sq->mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV
		     && !buffer_to_push)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Push header wasn't provided in LLQ mode\n");
		return ENA_COM_INVAL;
	}

	rc = ena_com_write_header_to_bounce(io_sq, buffer_to_push, header_len);
	if (unlikely(rc))
		return rc;

	rc = ena_com_create_and_store_tx_meta_desc(io_sq, ena_tx_ctx, &have_meta);
	if (unlikely(rc)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Failed to create and store tx meta desc\n");
		return rc;
	}

	/* If the caller doesn't want to send packets */
	if (unlikely(!num_bufs && !header_len)) {
		rc = ena_com_close_bounce_buffer(io_sq);
		if (unlikely(rc))
			ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
				    "Failed to write buffers to LLQ\n");
		*nb_hw_desc = io_sq->tail - start_tail;
		return rc;
	}

	desc = get_tx_sq_desc(io_sq);
	if (unlikely(!desc))
		return ENA_COM_FAULT;

	desc->len_ctrl = ENA_FIELD_PREP((u32)io_sq->phase,
					ENA_ETH_IO_TX_DESC_PHASE_MASK,
					ENA_ETH_IO_TX_DESC_PHASE_SHIFT);

	/* Set first desc when we don't have meta descriptor */
	if (!have_meta)
		desc->len_ctrl |= ENA_ETH_IO_TX_DESC_FIRST_MASK;

	desc->buff_addr_hi_hdr_sz = ENA_FIELD_PREP((u32)header_len,
						   ENA_ETH_IO_TX_DESC_HEADER_LENGTH_MASK,
						   ENA_ETH_IO_TX_DESC_HEADER_LENGTH_SHIFT);

	desc->len_ctrl |= ENA_ETH_IO_TX_DESC_COMP_REQ_MASK;

	/* Bits 0-9 */
	desc->meta_ctrl = ENA_FIELD_PREP((u32)ena_tx_ctx->req_id,
					 ENA_ETH_IO_TX_DESC_REQ_ID_LO_MASK,
					 ENA_ETH_IO_TX_DESC_REQ_ID_LO_SHIFT);

	desc->meta_ctrl |= ENA_FIELD_PREP(ena_tx_ctx->df,
					  ENA_ETH_IO_TX_DESC_DF_MASK,
					  ENA_ETH_IO_TX_DESC_DF_SHIFT);

	/* Bits 10-15 */
	desc->len_ctrl |= ENA_FIELD_PREP((ena_tx_ctx->req_id >> 10),
					 ENA_ETH_IO_TX_DESC_REQ_ID_HI_MASK,
					 ENA_ETH_IO_TX_DESC_REQ_ID_HI_SHIFT);

	if (ena_tx_ctx->meta_valid) {
		desc->meta_ctrl |= ENA_FIELD_PREP(ena_tx_ctx->tso_enable,
						  ENA_ETH_IO_TX_DESC_TSO_EN_MASK,
						  ENA_ETH_IO_TX_DESC_TSO_EN_SHIFT);
		desc->meta_ctrl |= ena_tx_ctx->l3_proto &
			ENA_ETH_IO_TX_DESC_L3_PROTO_IDX_MASK;
		desc->meta_ctrl |= ENA_FIELD_PREP(ena_tx_ctx->l4_proto,
						  ENA_ETH_IO_TX_DESC_L4_PROTO_IDX_MASK,
						  ENA_ETH_IO_TX_DESC_L4_PROTO_IDX_SHIFT);
		desc->meta_ctrl |= ENA_FIELD_PREP(ena_tx_ctx->l3_csum_enable,
						  ENA_ETH_IO_TX_DESC_L3_CSUM_EN_MASK,
						  ENA_ETH_IO_TX_DESC_L3_CSUM_EN_SHIFT);
		desc->meta_ctrl |= ENA_FIELD_PREP(ena_tx_ctx->l4_csum_enable,
						  ENA_ETH_IO_TX_DESC_L4_CSUM_EN_MASK,
						  ENA_ETH_IO_TX_DESC_L4_CSUM_EN_SHIFT);
		desc->meta_ctrl |= ENA_FIELD_PREP(ena_tx_ctx->l4_csum_partial,
						  ENA_ETH_IO_TX_DESC_L4_CSUM_PARTIAL_MASK,
						  ENA_ETH_IO_TX_DESC_L4_CSUM_PARTIAL_SHIFT);
	}

	for (i = 0; i < num_bufs; i++) {
		/* The first desc share the same desc as the header */
		if (likely(i != 0)) {
			rc = ena_com_sq_update_tail(io_sq);
			if (unlikely(rc)) {
				ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
					    "Failed to update sq tail\n");
				return rc;
			}

			desc = get_tx_sq_desc(io_sq);
			if (unlikely(!desc))
				return ENA_COM_FAULT;

			desc->len_ctrl |= ENA_FIELD_PREP((u32)io_sq->phase,
							 ENA_ETH_IO_TX_DESC_PHASE_MASK,
							 ENA_ETH_IO_TX_DESC_PHASE_SHIFT);
		}

		desc->len_ctrl |= ena_bufs->len &
			ENA_ETH_IO_TX_DESC_LENGTH_MASK;

		addr_hi = ((ena_bufs->paddr &
			GENMASK_ULL(io_sq->dma_addr_bits - 1, 32)) >> 32);

		desc->buff_addr_lo = (u32)ena_bufs->paddr;
		desc->buff_addr_hi_hdr_sz |= addr_hi &
			ENA_ETH_IO_TX_DESC_ADDR_HI_MASK;
		ena_bufs++;
	}

	/* set the last desc indicator */
	desc->len_ctrl |= ENA_ETH_IO_TX_DESC_LAST_MASK;

	rc = ena_com_sq_update_tail(io_sq);
	if (unlikely(rc)) {
		ena_trc_err(ena_com_io_sq_to_ena_dev(io_sq),
			    "Failed to update sq tail of the last descriptor\n");
		return rc;
	}

	rc = ena_com_close_bounce_buffer(io_sq);

	*nb_hw_desc = io_sq->tail - start_tail;
	return rc;
}

int ena_com_rx_pkt(struct ena_com_io_cq *io_cq,
		   struct ena_com_io_sq *io_sq,
		   struct ena_com_rx_ctx *ena_rx_ctx)
{
	struct ena_com_rx_buf_info *ena_buf = &ena_rx_ctx->ena_bufs[0];
	struct ena_eth_io_rx_cdesc_ext *cdesc = NULL;
	u16 cdesc_idx = 0;
	u16 nb_hw_desc;
	u16 i = 0;
	int rc;

	ENA_WARN(io_cq->direction != ENA_COM_IO_QUEUE_DIRECTION_RX,
		 ena_com_io_cq_to_ena_dev(io_cq), "wrong Q type");

	rc = ena_com_cdesc_rx_pkt_get(io_cq, &cdesc_idx, &nb_hw_desc);
	if (unlikely(rc != ENA_COM_OK))
		return rc;

	if (nb_hw_desc == 0) {
		ena_rx_ctx->descs = nb_hw_desc;
		return 0;
	}

	ena_trc_dbg(ena_com_io_cq_to_ena_dev(io_cq),
		    "Fetch rx packet: queue %u completed desc: %u\n",
		    io_cq->qid, nb_hw_desc);

	if (unlikely(nb_hw_desc > ena_rx_ctx->max_bufs)) {
		ena_trc_err(ena_com_io_cq_to_ena_dev(io_cq),
			    "Too many RX cdescs (%u) > MAX(%u)\n",
			    nb_hw_desc, ena_rx_ctx->max_bufs);
		return ENA_COM_NO_SPACE;
	}

	cdesc = ena_com_rx_cdesc_idx_to_ptr(io_cq, cdesc_idx);
	ena_rx_ctx->pkt_offset = cdesc->base.offset;

	do {
		ena_buf[i].len = cdesc->base.length;
		ena_buf[i].req_id = cdesc->base.req_id;

		if (++i >= nb_hw_desc)
			break;

		cdesc = ena_com_rx_cdesc_idx_to_ptr(io_cq, cdesc_idx + i);

	} while (1);

	/* Update SQ head ptr */
	io_sq->next_to_comp += nb_hw_desc;

	ena_trc_dbg(ena_com_io_cq_to_ena_dev(io_cq),
		    "Updating Queue %u, SQ head to: %u\n",
		    io_sq->qid, io_sq->next_to_comp);

	/* Get rx flags from the last pkt */
	ena_com_rx_set_flags(ena_rx_ctx, cdesc);

	if (unlikely(ena_com_is_extended_rx_cdesc(io_cq)))
		ena_rx_ctx->timestamp = (u64)cdesc->timestamp_low |
					(u64)cdesc->timestamp_high << 32;

	ena_trc_dbg(ena_com_io_cq_to_ena_dev(io_cq),
		    "l3_proto %d l4_proto %d l3_csum_err %d l4_csum_err %d hash %d frag %d cdesc_status %x\n",
		    ena_rx_ctx->l3_proto,
		    ena_rx_ctx->l4_proto,
		    ena_rx_ctx->l3_csum_err,
		    ena_rx_ctx->l4_csum_err,
		    ena_rx_ctx->hash,
		    ena_rx_ctx->frag,
		    cdesc->base.status);

	ena_rx_ctx->descs = nb_hw_desc;

	return 0;
}

int ena_com_add_single_rx_desc(struct ena_com_io_sq *io_sq,
			       struct ena_com_buf *ena_buf,
			       u16 req_id)
{
	struct ena_eth_io_rx_desc *desc;

	ENA_WARN(io_sq->direction != ENA_COM_IO_QUEUE_DIRECTION_RX,
		 ena_com_io_sq_to_ena_dev(io_sq), "wrong Q type");

	if (unlikely(!ena_com_sq_have_enough_space(io_sq, 1)))
		return ENA_COM_NO_SPACE;

	/* virt_addr allocation success is checked before calling this function */
	desc = get_sq_desc_regular_queue(io_sq);

	memset(desc, 0x0, sizeof(struct ena_eth_io_rx_desc));

	desc->length = ena_buf->len;

	desc->ctrl = ENA_ETH_IO_RX_DESC_FIRST_MASK |
		     ENA_ETH_IO_RX_DESC_LAST_MASK |
		     ENA_ETH_IO_RX_DESC_COMP_REQ_MASK |
		     ENA_FIELD_GET(io_sq->phase,
				   ENA_ETH_IO_RX_DESC_PHASE_MASK,
				   ENA_ZERO_SHIFT);

	desc->req_id = req_id;

	ena_trc_dbg(ena_com_io_sq_to_ena_dev(io_sq),
		    "Adding single RX desc, Queue: %u, req_id: %u\n",
		    io_sq->qid, req_id);

	desc->buff_addr_lo = (u32)ena_buf->paddr;
	desc->buff_addr_hi =
		((ena_buf->paddr & GENMASK_ULL(io_sq->dma_addr_bits - 1, 32)) >> 32);

	return ena_com_sq_update_reqular_queue_tail(io_sq);
}

bool ena_com_rx_cq_empty(struct ena_com_io_cq *io_cq)
{
	return (ena_com_get_next_rx_cdesc(io_cq) == NULL);
}

static struct ena_eth_io_tx_cdesc_ext *ena_com_get_next_tx_cdesc(struct ena_com_io_cq *io_cq)
{
	struct ena_eth_io_tx_cdesc_ext *cdesc;
	u8 expected_phase, cdesc_phase;
	u16 masked_head;

	masked_head = io_cq->head & (io_cq->q_depth - 1);
	expected_phase = io_cq->phase;

	cdesc = (struct ena_eth_io_tx_cdesc_ext *)
		((uintptr_t)io_cq->cdesc_addr.virt_addr +
		(masked_head * io_cq->cdesc_entry_size_in_bytes));

	/* When the current completion descriptor phase isn't the same as the
	 * expected, it means that the device didn't update this completion yet.
	 */
	cdesc_phase = ENA_FIELD_GET(READ_ONCE8(cdesc->base.flags),
				    ENA_ETH_IO_TX_CDESC_PHASE_MASK,
				    ENA_ZERO_SHIFT);
	if (cdesc_phase != expected_phase)
		return NULL;

	/* Make sure we read the rest of the descriptor after the phase bit
	 * has been read
	 */
	dma_rmb();

	return cdesc;
}

bool ena_com_tx_cq_empty(struct ena_com_io_cq *io_cq)
{
	return (ena_com_get_next_tx_cdesc(io_cq) == NULL);
}
