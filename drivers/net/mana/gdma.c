/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Microsoft Corporation
 */

#include <ethdev_driver.h>
#include <rte_io.h>

#include "mana.h"

uint8_t *
gdma_get_wqe_pointer(struct mana_gdma_queue *queue)
{
	uint32_t offset_in_bytes =
		(queue->head * GDMA_WQE_ALIGNMENT_UNIT_SIZE) &
		(queue->size - 1);

	DP_LOG(DEBUG, "txq sq_head %u sq_size %u offset_in_bytes %u",
	       queue->head, queue->size, offset_in_bytes);

	if (offset_in_bytes + GDMA_WQE_ALIGNMENT_UNIT_SIZE > queue->size)
		DP_LOG(ERR, "fatal error: offset_in_bytes %u too big",
		       offset_in_bytes);

	return ((uint8_t *)queue->buffer) + offset_in_bytes;
}

static uint32_t
write_dma_client_oob(uint8_t *work_queue_buffer_pointer,
		     const struct gdma_work_request *work_request,
		     uint32_t client_oob_size)
{
	uint8_t *p = work_queue_buffer_pointer;

	struct gdma_wqe_dma_oob *header = (struct gdma_wqe_dma_oob *)p;

	memset(header, 0, sizeof(struct gdma_wqe_dma_oob));
	header->num_sgl_entries = work_request->num_sgl_elements;
	header->inline_client_oob_size_in_dwords =
		client_oob_size / sizeof(uint32_t);
	header->client_data_unit = work_request->client_data_unit;

	DP_LOG(DEBUG, "queue buf %p sgl %u oob_h %u du %u oob_buf %p oob_b %u",
	       work_queue_buffer_pointer, header->num_sgl_entries,
	       header->inline_client_oob_size_in_dwords,
	       header->client_data_unit, work_request->inline_oob_data,
	       work_request->inline_oob_size_in_bytes);

	p += sizeof(struct gdma_wqe_dma_oob);
	if (work_request->inline_oob_data &&
	    work_request->inline_oob_size_in_bytes > 0) {
		memcpy(p, work_request->inline_oob_data,
		       work_request->inline_oob_size_in_bytes);
		if (client_oob_size > work_request->inline_oob_size_in_bytes)
			memset(p + work_request->inline_oob_size_in_bytes, 0,
			       client_oob_size -
			       work_request->inline_oob_size_in_bytes);
	}

	return sizeof(struct gdma_wqe_dma_oob) + client_oob_size;
}

static uint32_t
write_scatter_gather_list(uint8_t *work_queue_head_pointer,
			  uint8_t *work_queue_end_pointer,
			  uint8_t *work_queue_cur_pointer,
			  struct gdma_work_request *work_request)
{
	struct gdma_sgl_element *sge_list;
	struct gdma_sgl_element dummy_sgl[1];
	uint8_t *address;
	uint32_t size;
	uint32_t num_sge;
	uint32_t size_to_queue_end;
	uint32_t sge_list_size;

	DP_LOG(DEBUG, "work_queue_cur_pointer %p work_request->flags %x",
	       work_queue_cur_pointer, work_request->flags);

	num_sge = work_request->num_sgl_elements;
	sge_list = work_request->sgl;
	size_to_queue_end = (uint32_t)(work_queue_end_pointer -
				       work_queue_cur_pointer);

	if (num_sge == 0) {
		/* Per spec, the case of an empty SGL should be handled as
		 * follows to avoid corrupted WQE errors:
		 * Write one dummy SGL entry
		 * Set the address to 1, leave the rest as 0
		 */
		dummy_sgl[num_sge].address = 1;
		dummy_sgl[num_sge].size = 0;
		dummy_sgl[num_sge].memory_key = 0;
		num_sge++;
		sge_list = dummy_sgl;
	}

	sge_list_size = 0;
	{
		address = (uint8_t *)sge_list;
		size = sizeof(struct gdma_sgl_element) * num_sge;
		if (size_to_queue_end < size) {
			memcpy(work_queue_cur_pointer, address,
			       size_to_queue_end);
			work_queue_cur_pointer = work_queue_head_pointer;
			address += size_to_queue_end;
			size -= size_to_queue_end;
		}

		memcpy(work_queue_cur_pointer, address, size);
		sge_list_size = size;
	}

	DP_LOG(DEBUG, "sge %u address 0x%" PRIx64 " size %u key %u list_s %u",
	       num_sge, sge_list->address, sge_list->size,
	       sge_list->memory_key, sge_list_size);

	return sge_list_size;
}

/*
 * Post a work request to queue.
 */
int
gdma_post_work_request(struct mana_gdma_queue *queue,
		       struct gdma_work_request *work_req,
		       uint32_t *wqe_size_in_bu)
{
	uint32_t client_oob_size =
		work_req->inline_oob_size_in_bytes >
				INLINE_OOB_SMALL_SIZE_IN_BYTES ?
			INLINE_OOB_LARGE_SIZE_IN_BYTES :
			INLINE_OOB_SMALL_SIZE_IN_BYTES;

	uint32_t sgl_data_size = sizeof(struct gdma_sgl_element) *
			RTE_MAX((uint32_t)1, work_req->num_sgl_elements);
	uint32_t wqe_size =
		RTE_ALIGN(sizeof(struct gdma_wqe_dma_oob) +
				client_oob_size + sgl_data_size,
			  GDMA_WQE_ALIGNMENT_UNIT_SIZE);
	uint8_t *wq_buffer_pointer;
	uint32_t queue_free_units = queue->count - (queue->head - queue->tail);

	if (wqe_size / GDMA_WQE_ALIGNMENT_UNIT_SIZE > queue_free_units) {
		DP_LOG(DEBUG, "WQE size %u queue count %u head %u tail %u",
		       wqe_size, queue->count, queue->head, queue->tail);
		return -EBUSY;
	}

	DP_LOG(DEBUG, "client_oob_size %u sgl_data_size %u wqe_size %u",
	       client_oob_size, sgl_data_size, wqe_size);

	*wqe_size_in_bu = wqe_size / GDMA_WQE_ALIGNMENT_UNIT_SIZE;

	wq_buffer_pointer = gdma_get_wqe_pointer(queue);
	wq_buffer_pointer += write_dma_client_oob(wq_buffer_pointer, work_req,
						  client_oob_size);
	if (wq_buffer_pointer >= ((uint8_t *)queue->buffer) + queue->size)
		wq_buffer_pointer -= queue->size;

	write_scatter_gather_list((uint8_t *)queue->buffer,
				  (uint8_t *)queue->buffer + queue->size,
				  wq_buffer_pointer, work_req);

	queue->head += wqe_size / GDMA_WQE_ALIGNMENT_UNIT_SIZE;

	return 0;
}

union gdma_doorbell_entry {
	uint64_t     as_uint64;

	struct {
		uint64_t id	  : 24;
		uint64_t reserved    : 8;
		uint64_t tail_ptr    : 31;
		uint64_t arm	 : 1;
	} cq;

	struct {
		uint64_t id	  : 24;
		uint64_t wqe_cnt     : 8;
		uint64_t tail_ptr    : 32;
	} rq;

	struct {
		uint64_t id	  : 24;
		uint64_t reserved    : 8;
		uint64_t tail_ptr    : 32;
	} sq;

	struct {
		uint64_t id	  : 16;
		uint64_t reserved    : 16;
		uint64_t tail_ptr    : 31;
		uint64_t arm	 : 1;
	} eq;
}; /* HW DATA */

enum {
	DOORBELL_OFFSET_SQ = 0x0,
	DOORBELL_OFFSET_RQ = 0x400,
	DOORBELL_OFFSET_CQ = 0x800,
	DOORBELL_OFFSET_EQ = 0xFF8,
};

/*
 * Write to hardware doorbell to notify new activity.
 */
int
mana_ring_doorbell(void *db_page, enum gdma_queue_types queue_type,
		   uint32_t queue_id, uint32_t tail, uint8_t arm)
{
	uint8_t *addr = db_page;
	union gdma_doorbell_entry e = {};

	switch (queue_type) {
	case GDMA_QUEUE_SEND:
		e.sq.id = queue_id;
		e.sq.tail_ptr = tail;
		addr += DOORBELL_OFFSET_SQ;
		break;

	case GDMA_QUEUE_RECEIVE:
		e.rq.id = queue_id;
		e.rq.tail_ptr = tail;
		e.rq.wqe_cnt = arm;
		addr += DOORBELL_OFFSET_RQ;
		break;

	case GDMA_QUEUE_COMPLETION:
		e.cq.id = queue_id;
		e.cq.tail_ptr = tail;
		e.cq.arm = arm;
		addr += DOORBELL_OFFSET_CQ;
		break;

	default:
		DP_LOG(ERR, "Unsupported queue type %d", queue_type);
		return -1;
	}

	/* Ensure all writes are done before ringing doorbell */
	rte_wmb();

	DP_LOG(DEBUG, "db_page %p addr %p queue_id %u type %u tail %u arm %u",
	       db_page, addr, queue_id, queue_type, tail, arm);

	rte_write64(e.as_uint64, addr);
	return 0;
}

/*
 * Poll completion queue for completions.
 */
uint32_t
gdma_poll_completion_queue(struct mana_gdma_queue *cq,
			   struct gdma_comp *gdma_comp, uint32_t max_comp)
{
	struct gdma_hardware_completion_entry *cqe;
	uint32_t new_owner_bits, old_owner_bits;
	uint32_t cqe_owner_bits;
	uint32_t num_comp = 0;
	struct gdma_hardware_completion_entry *buffer = cq->buffer;

	while (num_comp < max_comp) {
		cqe = &buffer[cq->head % cq->count];
		new_owner_bits = (cq->head / cq->count) &
					COMPLETION_QUEUE_OWNER_MASK;
		old_owner_bits = (cq->head / cq->count - 1) &
					COMPLETION_QUEUE_OWNER_MASK;
		cqe_owner_bits = cqe->owner_bits;

		DP_LOG(DEBUG, "comp cqe bits 0x%x owner bits 0x%x",
			cqe_owner_bits, old_owner_bits);

		/* No new entry */
		if (cqe_owner_bits == old_owner_bits)
			break;

		if (cqe_owner_bits != new_owner_bits) {
			DRV_LOG(ERR, "CQ overflowed, ID %u cqe 0x%x new 0x%x",
				cq->id, cqe_owner_bits, new_owner_bits);
			break;
		}

		gdma_comp[num_comp].cqe_data = cqe->dma_client_data;
		num_comp++;

		cq->head++;

		DP_LOG(DEBUG, "comp new 0x%x old 0x%x cqe 0x%x wq %u sq %u head %u",
		       new_owner_bits, old_owner_bits, cqe_owner_bits,
		       cqe->wq_num, cqe->is_sq, cq->head);
	}

	/* Make sure the CQE owner bits are checked before we access the data
	 * in CQE
	 */
	rte_rmb();

	return num_comp;
}
