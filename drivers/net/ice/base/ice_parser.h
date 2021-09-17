/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_PARSER_H_
#define _ICE_PARSER_H_

struct ice_parser {
	struct ice_hw *hw; /* pointer to the hardware structure */
};

enum ice_status ice_parser_create(struct ice_hw *hw, struct ice_parser **psr);
void ice_parser_destroy(struct ice_parser *psr);
#endif /* _ICE_PARSER_H_ */
