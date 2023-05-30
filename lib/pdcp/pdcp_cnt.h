/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_CNT_H
#define PDCP_CNT_H

#include <rte_common.h>

#include "pdcp_entity.h"

uint32_t pdcp_cnt_bitmap_get_memory_footprint(const struct rte_pdcp_entity_conf *conf);
int pdcp_cnt_bitmap_create(struct entity_priv_dl_part *dl, uint32_t nb_elem,
			   void *bitmap_mem, uint32_t mem_size);

void pdcp_cnt_bitmap_set(struct pdcp_cnt_bitmap bitmap, uint32_t count);
bool pdcp_cnt_bitmap_is_set(struct pdcp_cnt_bitmap bitmap, uint32_t count);
void pdcp_cnt_bitmap_range_clear(struct pdcp_cnt_bitmap bitmap, uint32_t start, uint32_t stop);

uint16_t pdcp_cnt_get_bitmap_size(uint32_t pending_bytes);
void pdcp_cnt_report_fill(struct pdcp_cnt_bitmap bitmap, struct entity_state state,
			  uint8_t *data, uint16_t data_len);

#endif /* PDCP_CNT_H */
