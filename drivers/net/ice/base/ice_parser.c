/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#include "ice_common.h"

/**
 * ice_parser_create - create a parser instance
 * @hw: pointer to the hardware structure
 * @psr: output parameter for a new parser instance be created
 */
enum ice_status ice_parser_create(struct ice_hw *hw, struct ice_parser **psr)
{
	struct ice_parser *p;

	p = (struct ice_parser *)ice_malloc(hw, sizeof(struct ice_parser));

	if (!p)
		return ICE_ERR_NO_MEMORY;

	p->hw = hw;

	*psr = p;
	return ICE_SUCCESS;
}

/**
 * ice_parser_destroy - destroy a parser instance
 * @psr: pointer to a parser instance
 */
void ice_parser_destroy(struct ice_parser *psr)
{
	ice_free(psr->hw, psr);
}
