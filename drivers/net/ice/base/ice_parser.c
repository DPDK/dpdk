/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_common.h"
#include "ice_parser_util.h"

#define ICE_SEC_DATA_OFFSET			4
#define ICE_SID_RXPARSER_IMEM_ENTRY_SIZE	48

/**
 * ice_parser_sect_item_get - parse a item from a section
 * @sect_type: section type
 * @section: section object
 * @index: index of the item to get
 * @offset: dummy as prototype of ice_pkg_enum_entry's last parameter
 */
void *ice_parser_sect_item_get(u32 sect_type, void *section,
			       u32 index, u32 *offset)
{
	struct ice_pkg_sect_hdr *hdr;
	int data_off = ICE_SEC_DATA_OFFSET;
	int size;

	if (!section)
		return NULL;

	switch (sect_type) {
	case ICE_SID_RXPARSER_IMEM:
		size = ICE_SID_RXPARSER_IMEM_ENTRY_SIZE;
		break;
	default:
		return NULL;
	}

	hdr = (struct ice_pkg_sect_hdr *)section;
	if (index >= LE16_TO_CPU(hdr->count))
		return NULL;

	return (void *)((uintptr_t)section + data_off + index * size);
}

/**
 * ice_parser_create_table - create a item table from a section
 * @hw: pointer to the hardware structure
 * @sect_type: section type
 * @item_size: item size in byte
 * @length: number of items in the table to create
 * @item_get: the function will be parsed to ice_pkg_enum_entry
 * @parser_item: the function to parse the item
 */
void *ice_parser_create_table(struct ice_hw *hw, u32 sect_type,
			      u32 item_size, u32 length,
			      void *(*item_get)(u32 sect_type, void *section,
						u32 index, u32 *offset),
			      void (*parse_item)(struct ice_hw *hw, u16 idx,
						 void *item, void *data,
						 int size))
{
	struct ice_seg *seg = hw->seg;
	struct ice_pkg_enum state;
	u16 idx = 0;
	void *table;
	void *data;

	if (!seg)
		return NULL;

	table = ice_malloc(hw, item_size * length);
	if (!table) {
		ice_debug(hw, ICE_DBG_PARSER, "failed to allocate memory for table type %d.\n",
			  sect_type);
		return NULL;
	}

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);
	do {
		data = ice_pkg_enum_entry(seg, &state, sect_type, NULL,
					  item_get);
		seg = NULL;
		if (data) {
			struct ice_pkg_sect_hdr *hdr =
				(struct ice_pkg_sect_hdr *)state.sect;

			idx = hdr->offset + state.entry_idx;
			parse_item(hw, idx,
				   (void *)((uintptr_t)table + idx * item_size),
				   data, item_size);
		}
	} while (data);

	return table;
}

/**
 * ice_parser_create - create a parser instance
 * @hw: pointer to the hardware structure
 * @psr: output parameter for a new parser instance be created
 */
enum ice_status ice_parser_create(struct ice_hw *hw, struct ice_parser **psr)
{
	enum ice_status status;
	struct ice_parser *p;

	p = (struct ice_parser *)ice_malloc(hw, sizeof(struct ice_parser));

	if (!p)
		return ICE_ERR_NO_MEMORY;

	p->hw = hw;

	p->imem_table = ice_imem_table_get(hw);
	if (!p->imem_table) {
		status = ICE_ERR_PARAM;
		goto err;
	}

	*psr = p;
	return ICE_SUCCESS;
err:
	ice_parser_destroy(p);
	return status;
}

/**
 * ice_parser_destroy - destroy a parser instance
 * @psr: pointer to a parser instance
 */
void ice_parser_destroy(struct ice_parser *psr)
{
	ice_free(psr->hw, psr->imem_table);

	ice_free(psr->hw, psr);
}
