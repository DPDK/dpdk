/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_bitmap.h>
#include <rte_pdcp.h>

#include "pdcp_cnt.h"
#include "pdcp_ctrl_pdu.h"
#include "pdcp_entity.h"

#define SLAB_BYTE_SIZE (RTE_BITMAP_SLAB_BIT_SIZE / 8)

uint32_t
pdcp_cnt_bitmap_get_memory_footprint(const struct rte_pdcp_entity_conf *conf)
{
	uint32_t n_bits = pdcp_window_size_get(conf->pdcp_xfrm.sn_size);

	return rte_bitmap_get_memory_footprint(n_bits);
}

int
pdcp_cnt_bitmap_create(struct entity_priv_dl_part *dl, uint32_t nb_elem,
		       void *bitmap_mem, uint32_t mem_size)
{
	dl->bitmap.bmp = rte_bitmap_init(nb_elem, bitmap_mem, mem_size);
	if (dl->bitmap.bmp == NULL)
		return -EINVAL;

	dl->bitmap.size = nb_elem;

	return 0;
}

void
pdcp_cnt_bitmap_set(struct pdcp_cnt_bitmap bitmap, uint32_t count)
{
	rte_bitmap_set(bitmap.bmp, count % bitmap.size);
}

bool
pdcp_cnt_bitmap_is_set(struct pdcp_cnt_bitmap bitmap, uint32_t count)
{
	return rte_bitmap_get(bitmap.bmp, count % bitmap.size);
}

void
pdcp_cnt_bitmap_range_clear(struct pdcp_cnt_bitmap bitmap, uint32_t start, uint32_t stop)
{
	uint32_t i;

	for (i = start; i < stop; i++)
		rte_bitmap_clear(bitmap.bmp, i % bitmap.size);
}

uint16_t
pdcp_cnt_get_bitmap_size(uint32_t pending_bytes)
{
	/*
	 * Round up bitmap size to slab size to operate only on slabs sizes, instead of individual
	 * bytes
	 */
	return RTE_ALIGN_MUL_CEIL(pending_bytes, SLAB_BYTE_SIZE);
}

static __rte_always_inline uint64_t
leftover_get(uint64_t slab, uint32_t shift, uint64_t mask)
{
	return (slab & mask) << shift;
}

void
pdcp_cnt_report_fill(struct pdcp_cnt_bitmap bitmap, struct entity_state state,
		     uint8_t *data, uint16_t data_len)
{
	uint64_t slab = 0, next_slab = 0, leftover;
	uint32_t zeros, report_len, diff;
	uint32_t slab_id, next_slab_id;
	uint32_t pos = 0, next_pos = 0;

	const uint32_t start_count = state.rx_deliv + 1;
	const uint32_t nb_slabs = bitmap.size / RTE_BITMAP_SLAB_BIT_SIZE;
	const uint32_t nb_data_slabs = data_len / SLAB_BYTE_SIZE;
	const uint32_t start_slab_id = start_count / RTE_BITMAP_SLAB_BIT_SIZE;
	const uint32_t stop_slab_id = (start_slab_id + nb_data_slabs) % nb_slabs;
	const uint32_t shift = start_count % RTE_BITMAP_SLAB_BIT_SIZE;
	const uint32_t leftover_shift = shift ? RTE_BITMAP_SLAB_BIT_SIZE - shift : 0;
	const uint8_t *data_end = RTE_PTR_ADD(data, data_len + SLAB_BYTE_SIZE);

	/* NOTE: Mask required to workaround case - when shift is not needed */
	const uint64_t leftover_mask = shift ? ~0 : 0;

	/* NOTE: implement scan init at to set custom position */
	__rte_bitmap_scan_init(bitmap.bmp);
	while (true) {
		assert(rte_bitmap_scan(bitmap.bmp, &pos, &slab) == 1);
		slab_id = pos / RTE_BITMAP_SLAB_BIT_SIZE;
		if (slab_id >= start_slab_id)
			break;
	}

	report_len = nb_data_slabs;

	if (slab_id > start_slab_id) {
		/* Zero slabs at beginning */
		zeros = (slab_id - start_slab_id - 1) * SLAB_BYTE_SIZE;
		memset(data, 0, zeros);
		data = RTE_PTR_ADD(data, zeros);
		leftover = leftover_get(slab, leftover_shift, leftover_mask);
		memcpy(data, &leftover, SLAB_BYTE_SIZE);
		data = RTE_PTR_ADD(data, SLAB_BYTE_SIZE);
		report_len -= (slab_id - start_slab_id);
	}

	while (report_len) {
		rte_bitmap_scan(bitmap.bmp, &next_pos, &next_slab);
		next_slab_id = next_pos / RTE_BITMAP_SLAB_BIT_SIZE;
		diff = (next_slab_id + nb_slabs - slab_id) % nb_slabs;

		/* If next_slab_id == slab_id - overlap */
		diff += !(next_slab_id ^ slab_id) * nb_slabs;

		/* Size check - next slab is outsize of size range */
		if (diff > report_len) {
			next_slab = 0;
			next_slab_id = stop_slab_id;
			diff = report_len;
		}

		report_len -= diff;

		/* Calculate gap between slabs, taking wrap around into account */
		zeros = (next_slab_id + nb_slabs - slab_id - 1) % nb_slabs;
		if (zeros) {
			/* Non continues slabs, align them individually */
			slab >>= shift;
			memcpy(data, &slab, SLAB_BYTE_SIZE);
			data = RTE_PTR_ADD(data, SLAB_BYTE_SIZE);

			/* Fill zeros between slabs */
			zeros = (zeros - 1) * SLAB_BYTE_SIZE;
			memset(data, 0, zeros);
			data = RTE_PTR_ADD(data, zeros);

			/* Align beginning of next slab */
			leftover = leftover_get(next_slab, leftover_shift, leftover_mask);
			memcpy(data, &leftover, SLAB_BYTE_SIZE);
			data = RTE_PTR_ADD(data, SLAB_BYTE_SIZE);
		} else {
			/* Continues slabs, combine them */
			uint64_t new_slab = (slab >> shift) |
					leftover_get(next_slab, leftover_shift, leftover_mask);
			memcpy(data, &new_slab, SLAB_BYTE_SIZE);
			data = RTE_PTR_ADD(data, SLAB_BYTE_SIZE);
		}

		slab = next_slab;
		pos = next_pos;
		slab_id = next_slab_id;

	};

	assert(data < data_end);
}
