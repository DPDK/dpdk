/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_PARSER_UTIL_H_
#define _ICE_PARSER_UTIL_H_

#include "ice_imem.h"

struct ice_pkg_sect_hdr {
	__le16 count;
	__le16 offset;
};

void *ice_parser_sect_item_get(u32 sect_type, void *section,
			       u32 index, u32 *offset);

void *ice_parser_create_table(struct ice_hw *hw, u32 sect_type,
			      u32 item_size, u32 length,
			      void *(*handler)(u32 sect_type, void *section,
					       u32 index, u32 *offset),
			      void (*parse_item)(struct ice_hw *hw, u16 idx,
						 void *item, void *data,
						 int size));
#endif /* _ICE_PARSER_UTIL_H_ */
